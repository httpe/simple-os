#include <stdio.h>
#include <kernel/multiboot.h>
#include <kernel/panic.h>
#include <kernel/memory_bitmap.h>

// Memory bitmap
#define uint32combine(high,low) ((((uint64_t) (high)) << 32) + (uint64_t) (low))

// Trick to access linker script variable: Use its address not content, declaring it as array give us address naturally 
// Ref: https://stackoverflow.com/questions/48561217/how-to-get-value-of-variable-defined-in-ld-linker-script-from-c
extern char KERNEL_PHYSICAL_START[], KERNEL_PHYSICAL_END[];

// A bitset of frames - used or free.
static uint32_t frames[ARRAY_INDEX_FROM_FRAME_INDEX(N_FRAMES)];

// Last known allocated frame, not necessarily correct 
static uint32_t last_allocated_frame_idx = 0;

// Set a bit in the frames bitset
void set_frame(uint32_t frame_idx) {
    // uint32_t frame_idx = FRAME_INDEX_FROM_ADDR(physical_addr);
    uint32_t idx = ARRAY_INDEX_FROM_FRAME_INDEX(frame_idx);
    uint32_t off = BIT_OFFSET_FROM_FRAME_INDEX(frame_idx);
    frames[idx] |= (0x1 << off);
    last_allocated_frame_idx = frame_idx;
}

// Clear a bit in the frames bitset
void clear_frame(uint32_t frame_idx) {
    // uint32_t frame_idx = FRAME_INDEX_FROM_ADDR(physical_addr);
    uint32_t idx = ARRAY_INDEX_FROM_FRAME_INDEX(frame_idx);
    uint32_t off = BIT_OFFSET_FROM_FRAME_INDEX(frame_idx);
    frames[idx] &= ~(0x1 << off);
}

// Test if a bit is set.
uint32_t test_frame(uint32_t frame_idx) {
    // uint32_t frame_idx = FRAME_INDEX_FROM_ADDR(physical_addr);
    uint32_t idx = ARRAY_INDEX_FROM_FRAME_INDEX(frame_idx);
    uint32_t off = BIT_OFFSET_FROM_FRAME_INDEX(frame_idx);
    return (frames[idx] & (0x1 << off));
}

// Find the first free frame.
uint32_t first_free_frame() {
    uint32_t frame_idx = (last_allocated_frame_idx + 1) % N_FRAMES;
    for(; frame_idx != last_allocated_frame_idx; frame_idx = (frame_idx + 1) % N_FRAMES) {
        uint32_t i = ARRAY_INDEX_FROM_FRAME_INDEX(frame_idx);
        uint32_t j = BIT_OFFSET_FROM_FRAME_INDEX(frame_idx);
        if (frames[i] != 0xFFFFFFFF) // nothing free, exit early.
        {
            // at least one bit is free here.
            for (j = 0; j < 32; j++)             {
                uint32_t toTest = 0x1 << j;
                if (!(frames[i] & toTest))                 {
                    return i * 4 * 8 + j; // return the index of the first free frame
                }
            }
        }
    }

    PANIC("No free frame!");
}

void initialize_bitmap(uint32_t mbt_physical_addr) {
    for (int i = 0;i < ARRAY_INDEX_FROM_FRAME_INDEX(N_FRAMES);i++) {
        // Initialize all memory to be used/reserved, we will clear for available memory below
        frames[i] = 0xFFFFFFFF;
    }

    // Get memory map
    // Get the virtual address of the Multiboot info structure
    printf("PhysicalAddr(Multiboot Info): 0x%x\n", mbt_physical_addr);
    multiboot_info_t* mbt = (multiboot_info_t*)(mbt_physical_addr + 0xC0000000);
    printf("Multiboot Flag: 0x%x\n", mbt->flags);

    multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)(mbt->mmap_addr + 0xC0000000);
    uint64_t memory_size = 0, memory_available = 0;
    uint64_t mem_block_addr, mem_block_len;
    uint64_t frame_idx, frame_idx_end;
    while (((uint32_t)mmap) < 0xC0000000 + mbt->mmap_addr + mbt->mmap_length) {
        printf("Size: 0x%x; Base: 0x%x:%x; Length: 0x%x:%x; Type: 0x%x\n",
            mmap->size,
            mmap->addr_high, mmap->addr_low,
            mmap->len_high, mmap->len_low,
            mmap->type);

        // Detect memory size
        mem_block_addr = uint32combine(mmap->addr_high, mmap->addr_low);
        mem_block_len = uint32combine(mmap->len_high, mmap->len_low);
        if ((mem_block_addr + mem_block_len) > memory_size) {
            memory_size = mem_block_addr + mem_block_len;
        }
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
            memory_available += mem_block_len;
        }

        // Clear memory bitmap for available area
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
            frame_idx = FRAME_INDEX_FROM_ADDR(mem_block_addr);
            frame_idx_end = FRAME_INDEX_FROM_ADDR(mem_block_addr + mem_block_len - 1); // memory block size counted in number of frames
            // Our bit map only support 4GiB of memory
            // Addressing over 4GiB memory in 32bit architecture needs PAE
            // Ref: https://wiki.osdev.org/Setting_Up_Paging_With_PAE
            while (frame_idx < N_FRAMES && frame_idx <= frame_idx_end) {
                clear_frame(frame_idx);
                frame_idx++;
            }
        }

        mmap = (multiboot_memory_map_t*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
    }

    printf("Memory size: %d MiB detected; %d MiB free\n", (uint32_t)(memory_size / (1024 * 1024)), (uint32_t)(memory_available / (1024 * 1024)));

    // Set memory bitmap for reserved area
    // Spliting the operations in multiple pass is to ensure overlapped area are set to reserved
    mmap = (multiboot_memory_map_t*)(mbt->mmap_addr + 0xC0000000);
    while (((uint32_t)mmap) < 0xC0000000 + mbt->mmap_addr + mbt->mmap_length) {
        mem_block_addr = uint32combine(mmap->addr_high, mmap->addr_low);
        mem_block_len = uint32combine(mmap->len_high, mmap->len_low);
        if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE) {
            frame_idx = FRAME_INDEX_FROM_ADDR(mem_block_addr);
            frame_idx_end = FRAME_INDEX_FROM_ADDR(mem_block_addr + mem_block_len - 1); // memory block size counted in number of frames
            // printf("Frame reserved: 0x%x - 0x%x\n", (uint32_t) frame_idx, (uint32_t) frame_idx_end);
            while (frame_idx < N_FRAMES && frame_idx <= frame_idx_end) {
                set_frame(frame_idx);
                frame_idx++;
            }
        }
        mmap = (multiboot_memory_map_t*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
    }

    // Set bit map for kernel physical space
    uint32_t kernel_frame_start = FRAME_INDEX_FROM_ADDR((uint32_t)KERNEL_PHYSICAL_START);
    uint32_t kernel_frame_end = FRAME_INDEX_FROM_ADDR((uint32_t)KERNEL_PHYSICAL_END);
    for (uint32_t idx = kernel_frame_start; idx <= kernel_frame_end; idx++)     {
        set_frame(idx);
    }
    printf("Kernel Frame Reserved: 0x%x - 0x%x", kernel_frame_start, kernel_frame_end);
}