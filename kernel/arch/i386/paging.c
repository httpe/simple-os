#include "paging.h"
#include "isr.h"
#include "memory_bitmap.h"
#include <kernel/multiboot.h>
#include <stdio.h>
#include <string.h>

#define uint32combine(high,low) ((((uint64_t) (high)) << 32) + (uint64_t) (low))

#define PAGE_DIR_SIZE 1024
#define PAGE_TABLE_SIZE 1024
// By using the recursive page directory trick, we can access page dir entry via
// virtual address [10 bits of 1;10bits of 1;0, 4, 8 etc. 12bits of page dir index * 4]
// where [10 bits of 1;10bits of 1;12bits of 0] = 0xFFFFF000
#define PAGE_DIR_PTR ((page_directory_entry_t*) 0xFFFFF000)
// To access page table entries, do similarly [10 bits of 1; 10bits of page dir index for the table; 0, 4, 8 etc. 12bits of page table index * 4]
#define PAGE_TABLE_PTR(idx) ((page_t*) (0xFFC00000 + ((idx) << 12)))
// We can also get page directory's physical address by acessing it's last entry, which point to itself, thus being recursive
#define PAGE_DIR_PHYSICAL_ADDR (((page_directory_entry_t*) 0xFFFFF000)[1023].page_table_addr << 12)

// Trick to access linker script variable: Use its address not content, declaring it as array give us address naturally 
// Ref: https://stackoverflow.com/questions/48561217/how-to-get-value-of-variable-defined-in-ld-linker-script-from-c
extern char KERNEL_PHYSICAL_START[], KERNEL_PHYSICAL_END[];

#define PAGE_FAULT_INTERRUPT 14
// #define PAGE_TO_MAP 4
// page_table_t page_tables[PAGE_TO_MAP] __attribute__((aligned(4096)));
// page_directory_t first_page_directory __attribute__((aligned(4096)));



static void page_fault_callback(registers_t *regs)
{
    uint32_t cr2;
    asm volatile("mov %%cr2, %0": "=r"(cr2));
    printf("KERNEL PANIC: PAGE FAULT! Address: 0x%x", cr2);
    while (1);
}

void install_page_fault_handler()
{
    register_interrupt_handler(PAGE_FAULT_INTERRUPT, page_fault_callback);
}

// Function to allocate a frame.
void alloc_frame(page_t *page, int is_kernel, int is_writeable)
{
    if (page->frame != 0)
    {
        return; // Frame was already allocated, return straight away.
    }
    else
    {
        uint32_t idx = first_free_frame(); // idx is now the index of the first free frame.
        set_frame(idx);                    // this frame is now ours!
        page->present = 1;                 // Mark it as present.
        page->rw = (is_writeable) ? 1 : 0; // Should the page be writeable?
        page->user = (is_kernel) ? 0 : 1;  // Should the page be user-mode?
        page->frame = idx;                 // the frame index is the frame address shift right 12 bits
    }
}

// Function to deallocate a frame.
void free_frame(page_t *page)
{
    if (page->frame == 0)
    {
        return; // The given page didn't actually have an allocated frame!
    }
    else
    {
        clear_frame(page->frame); // Frame is now free again.
        page->frame = 0x0;        // Page now doesn't have a frame.
    }
}

uint32_t find_contiguous_virtual_addr(size_t page_count) {
    // Find a contiguous virtual address space to map from
    uint32_t contiguous_page_count = 0;
    for(uint32_t page_dir_idx=0; page_dir_idx < PAGE_DIR_SIZE; page_dir_idx++)
    {
        if(PAGE_DIR_PTR[page_dir_idx].present == 0)
        {
            if(contiguous_page_count + PAGE_TABLE_SIZE >= page_count) {
                return page_dir_idx*PAGE_TABLE_SIZE - contiguous_page_count;
            }
            contiguous_page_count += PAGE_TABLE_SIZE;
            continue;
        }
        for(uint32_t page_table_idx=0; page_table_idx < PAGE_TABLE_SIZE; page_table_idx++)
        {
            if(PAGE_TABLE_PTR(page_dir_idx)[page_table_idx].present == 0) {
                if(contiguous_page_count + 1 >= page_count) {
                    return page_dir_idx*PAGE_TABLE_SIZE + page_table_idx - contiguous_page_count;
                }
                contiguous_page_count++;
            } else {
                contiguous_page_count = 0;
            }
        }
    }

    printf("KERNEL PANIC: Find contiguous VA failed\n");
    while(1);

}

uint32_t kmalloc(size_t size)
{
    if(size == 0)
    {
        return 0;
    }
    uint32_t page_count = size / 4096;
    if(size % 4096 != 0) {
        page_count++;
    }

    uint32_t map_from_page_index = find_contiguous_virtual_addr(page_count);
    printf("kmalloc(map_from_page_index): %d\n", map_from_page_index);
    uint32_t page_allocated = 0;

    uint32_t page_dir_idx = map_from_page_index / PAGE_TABLE_SIZE;
    uint32_t page_table_idx = map_from_page_index % PAGE_TABLE_SIZE;
    while(page_allocated < page_count && page_dir_idx < PAGE_DIR_SIZE) {
        if(PAGE_DIR_PTR[page_dir_idx].present == 0) {
            // Page table not allocated, create it first
            uint32_t idx = first_free_frame(); // idx is now the index of the first free frame.
            set_frame(idx);                    // this frame is now ours!
            page_directory_entry_t page_dir_entry = {.present = 1, .rw = 1, .user = 0, .page_table_addr = idx};
            PAGE_DIR_PTR[page_dir_idx] = page_dir_entry;
            // flush
            asm volatile("mov %0, %%cr3"
                :
                : "r"(PAGE_DIR_PHYSICAL_ADDR));
            memset(PAGE_TABLE_PTR(page_dir_idx), 0, sizeof(page_t)*PAGE_TABLE_SIZE);
        }
        if(!PAGE_TABLE_PTR(page_dir_idx)[page_table_idx].present) { // This should always be true
            // Not present means it hasn't been mapped to physical space
            uint32_t idx = first_free_frame(); // idx is now the index of the first free frame.
            set_frame(idx);                    // this frame is now ours!
            page_t page = {.present = 1, .user = 0, .rw = 1, .frame = idx};
            PAGE_TABLE_PTR(page_dir_idx)[page_table_idx] = page;
            // flush
            asm volatile("mov %0, %%cr3"
                :
                : "r"(PAGE_DIR_PHYSICAL_ADDR));
            page_allocated++;
            printf("Page allocated: PD[%d]:PT[%d]:Frame[%d]\n", page_dir_idx, page_table_idx, idx);
        }
        // printf("PD[%d]:PT[%d]:Frame[%d]\n", page_dir_idx, page_table_idx, PAGE_TABLE_PTR(page_dir_idx)[page_table_idx].frame);
        page_table_idx++;

        if(page_table_idx >= PAGE_TABLE_SIZE) {
            page_dir_idx++;
            page_table_idx = 0;
        }
    }
    
    if(page_allocated == page_count) {
        // return the starting address of the allocated area
        return ADDR_FROM_FRAME_INDEX(map_from_page_index);
    } else {
        printf("KERNEL PANIC: Memory allocation failed\n");
        while(1);
    }
}

void initialize_paging(uint32_t mbt_physical_addr)
{
    install_page_fault_handler();

	// Get memory map
	// Get the virtual address of the Multiboot info structure
	printf("PhysicalAddr(Multiboot Info): 0x%x\n", mbt_physical_addr);
	multiboot_info_t* mbt = (multiboot_info_t*) (mbt_physical_addr + 0xC0000000);
	printf("Multiboot Flag: 0x%x\n", mbt->flags);
	multiboot_memory_map_t* mmap = (multiboot_memory_map_t*) (mbt->mmap_addr + 0xC0000000);

    uint64_t memory_size = 0, memory_available = 0;
    uint64_t frame_idx, frame_idx_end;
    uint64_t mem_block_addr, mem_block_len;
	while( ((uint32_t) mmap) < 0xC0000000 + mbt->mmap_addr + mbt->mmap_length) {
		printf("Size: 0x%x; Base: 0x%x:%x; Length: 0x%x:%x; Type: 0x%x\n",
			mmap->size,
			mmap->addr_high, mmap->addr_low, 
			mmap->len_high, mmap->len_low,
			mmap->type);

        // Detect memory size
        mem_block_addr = uint32combine(mmap->addr_high, mmap->addr_low);
        mem_block_len = uint32combine(mmap->len_high, mmap->len_low);
        if((mem_block_addr + mem_block_len) > memory_size) {
            memory_size = mem_block_addr + mem_block_len;
        }
        if(mmap->type == MULTIBOOT_MEMORY_AVAILABLE){
            memory_available += mem_block_len;
        }
        // Set memory bitmap for reserved area
        if(mmap->type != MULTIBOOT_MEMORY_AVAILABLE) {
            frame_idx = FRAME_INDEX_FROM_ADDR(mem_block_addr);
            frame_idx_end = FRAME_INDEX_FROM_ADDR(mem_block_addr + mem_block_len - 1); // memory block size counted in number of frames
            // printf("Frame reserved: 0x%x - 0x%x\n", (uint32_t) frame_idx, (uint32_t) frame_idx_end);
            // Our bit map only support 4GiB of memory
            // Addressing over 4GiB memory in 32bit architecture needs PAE
            // Ref: https://wiki.osdev.org/Setting_Up_Paging_With_PAE
            while(frame_idx < N_FRAMES && frame_idx <= frame_idx_end) {
                set_frame(frame_idx);
                frame_idx++;
            }
        }

		mmap = (multiboot_memory_map_t*) ((uint32_t) mmap + mmap->size + sizeof(mmap->size));
	}
    printf("Memory size: %d MiB detected; %d MiB free\n", (uint32_t) (memory_size/(1024*1024)), (uint32_t) (memory_available/(1024*1024)));

    // Set bit map for kernel physical space
    uint32_t kernel_frame_start = FRAME_INDEX_FROM_ADDR((uint32_t) KERNEL_PHYSICAL_START);
    uint32_t kernel_frame_end = FRAME_INDEX_FROM_ADDR((uint32_t) KERNEL_PHYSICAL_END);
    for(size_t idx=kernel_frame_start; idx<=kernel_frame_end; idx++)
    {
        set_frame(idx);
    }
    printf("Kernel Frame Reserved: 0x%x - 0x%x", kernel_frame_start, kernel_frame_end);

    printf("Boot page dir physical addr: 0x%x\n", PAGE_DIR_PHYSICAL_ADDR);
    page_directory_entry_t page_dir_entry_0 = PAGE_DIR_PTR[0xC0000000 >> 22];
    printf("Boot page table physical addr: 0x%x\n", page_dir_entry_0.page_table_addr << 12);
    page_t page_table_entry_0 = PAGE_TABLE_PTR(0xC0000000 >> 22)[0];
    printf("Boot page table entry 0 point to physical addr: 0x%x\n", page_table_entry_0.frame << 12);

    uint32_t alloc_addr = kmalloc(4096);
    printf("Allocated an uint32_t[4096] array at virtual address: 0x%x\n", alloc_addr);
    uint8_t *array = (uint8_t*) alloc_addr;
    array[0] = 1;
    array[4095] = 10;
    printf("Array[0]=%d; Array[4095]=%d\n", array[0], array[4095]);

    uint32_t alloc_addr2 = kmalloc(4096);
    printf("Allocated second uint32_t[4096] array at virtual address: 0x%x\n", alloc_addr2);
    uint8_t *array2 = (uint8_t*) alloc_addr2;
    array2[0] = 6;
    array2[4095] = 9;
    printf("Array2[0]=%d; Array2[4095]=%d\n", array2[0], array2[4095]);
}

// page_t *get_page(uint32_t address, int make, page_directory_t *dir)
// {
//     // Turn the address into an (page) index.
//     address /= 0x1000;
//     // Find the page table containing this address, i.e. the page table index in page directory
//     uint32_t table_idx = address / 1024;
//     if (dir->tables[table_idx]) // If this table is already assigned
//     {
//         return &dir->tables[table_idx]->pages[address % 1024];
//     }
//     else if (make)
//     {
//         uint32_t tmp;
//         dir->tables[table_idx] = (page_table_t *)kmalloc_ap(sizeof(page_table_t), &tmp);
//         memset(dir->tables[table_idx], 0, 0x1000);
//         dir->tablesPhysical[table_idx] = tmp | 0x7; // PRESENT, RW, US.
//         return &dir->tables[table_idx]->pages[address % 1024];
//     }
//     else
//     {
//         return 0;
//     }
// }


// void initialize_paging()
// {
//     //set each entry to not present
//     unsigned int i;
//     for (i = 0; i < 1024; i++)
//     {
//         // This sets the following flags to the pages:
//         //   Supervisor: Only kernel-mode can access them
//         //   Write Enabled: It can be both read from and written to
//         //   Not Present: The page table is not present
//         first_page_directory.tablesPhysical[i] = (page_directory_entry_t){.present = 0, .rw = 1, .user = 0, .page_table_addr = 0};
//     }

//     // holds the physical address where we want to start mapping these pages to.
//     // in this case, we want to map these pages to the very beginning of memory.

//     //we will fill all 1024 entries in the table, mapping 4 megabytes
//     for (unsigned int j = 0; j < PAGE_TO_MAP; j++)
//     {
//         for (i = 0; i < 1024; i++)
//         {
//             // As the address is page aligned, it will always leave 12 bits zeroed.
//             // Those bits are used by the attributes ;)
//             // attributes: supervisor level, read/write, present.
//             // Frame: Identity Paging
//             page_tables[j].pages[i] = (page_t){.present = 1, .user = 0, .rw = 1, .frame = j * 1024 + i};
//         }
//         first_page_directory.tablesPhysical[j] = (page_directory_entry_t){.present = 1, .rw = 1, .user = 0, .page_table_addr = ((uint32_t)&page_tables[j]) >> 12};
//     }

//     first_page_directory.physicalAddr = (uint32_t)first_page_directory.tablesPhysical;

//     asm volatile("mov %0, %%cr3"
//                  :
//                  : "r"(&first_page_directory.tablesPhysical));
//     uint32_t cr0;
//     asm volatile("mov %%cr0, %0"
//                  : "=r"(cr0));
//     cr0 |= 0x80000000; // Enable paging!
//     asm volatile("mov %0, %%cr0"
//                  :
//                  : "r"(cr0));
// }