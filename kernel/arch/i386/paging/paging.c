#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <kernel/panic.h>
#include <common.h>
#include <kernel/memory_bitmap.h>
#include <kernel/paging.h>
#include <kernel/multiboot.h>
#include <arch/i386/kernel/isr.h>
#include <arch/i386/kernel/segmentation.h>
#include <arch/i386/kernel/cpu.h>

// Ref: https://blog.inlow.online/2019/01/21/Paging/
// Ref: http://www.jamesmolloy.co.uk/tutorial_html/6.-Paging.html

// Entries per page directory
#define PAGE_DIR_SIZE 1024
// Entries per page table
#define PAGE_TABLE_SIZE 1024

#define PAGE_FAULT_INTERRUPT 14

typedef struct page
{
   uint32_t present    : 1;   // Page present in memory
   uint32_t rw         : 1;   // Read-only if clear, readwrite if set
   uint32_t user       : 1;   // Supervisor level only if clear
   uint32_t accessed   : 1;   // Has the page been accessed since last refresh?
   uint32_t dirty      : 1;   // Has the page been written to since last refresh?
   uint32_t unused     : 7;   // Amalgamation of unused and reserved bits
   uint32_t frame      : 20;  // Frame address (shifted right 12 bits)
} __attribute__((packed)) page_t;

typedef struct page_directory_entry
{
   uint32_t present         : 1;   // Page present in memory
   uint32_t rw              : 1;   // Read-only if clear, readwrite if set
   uint32_t user            : 1;   // Supervisor level only if clear
   uint32_t write_through   : 1;   // If the bit is set, write-through caching is enabled. If not, then write-back is enabled instead.
   uint32_t cache_disabled  : 1;   // If the bit is set, the page will not be cached. Otherwise, it will be.
   uint32_t accessed        : 1;   // Has the page been accessed since last refresh?
   uint32_t zero            : 1;   // Always zero
   uint32_t page_size       : 1;   // If the bit is set, then pages are 4 MiB in size. Otherwise, they are 4 KiB. Please note that 4-MiB pages require PSE to be enabled.
   uint32_t ignored         : 1;   // Ignored bit
   uint32_t available       : 3;   // Available to OS
   uint32_t page_table_frame : 20; // Physical address to the page table (shifted right 12 bits)
} pde;

// Accessing current page directory and page tables
// By using the recursive page directory trick, we can access page dir entry via
// virtual address [10 bits of 1;10bits of 1;0, 4, 8 etc. 12bits of page dir index * 4]
// where [10 bits of 1;10bits of 1;12bits of 0] = 0xFFFFF000
#define PAGE_DIR_PTR ((pde*) 0xFFFFF000)
// To access page table entries, do similarly [10 bits of 1; 10bits of page dir index for the table; 0, 4, 8 etc. 12bits of page table index * 4]
#define PAGE_TABLE_PTR(idx) ((page_t*) (0xFFC00000 + ((idx) << 12)))
// We can also get page directory's physical address by acessing it's last entry, which point to itself, thus being recursive
#define PAGE_DIR_PHYSICAL_ADDR (((pde*) 0xFFFFF000)[1023].page_table_frame << 12)


// Declare internal utility functions
static uint32_t find_contiguous_free_pages(pde* page_dir, size_t page_count, bool is_kernel);
static uint unmap_pages_from(pde* page_dir, uint page_index, uint page_count, bool free_frame, bool skip_unmapped);


// Load GDT
static inline void lgdt(struct segdesc *p, uint16_t size)
{
  volatile uint16_t pd[3];

  pd[0] = size-1;
  pd[1] = (uint32_t)p;
  pd[2] = (uint32_t)p >> 16;

  asm volatile("lgdt (%0)" : : "r" (pd));
}

// Load TSS selector into TR register
static inline void ltr(uint16_t sel)
{
  asm volatile("ltr %0" : : "r" (sel));
}

// Initialize GDT with user space entries and a slot for TSS
static void init_gdt()
{
    cpu* cpu = curr_cpu();
    cpu->gdt[SEG_NULL] = (segdesc) {0};
    cpu->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
    cpu->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
    cpu->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
    cpu->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
    // TSS will be setup later, but we tell CPU there will one through size argument of lgdt (NSEGS)
    lgdt(cpu->gdt, sizeof(cpu->gdt));
}

// Flush TLB (translation lookaside buffer) for a single page
// Need flushing when a page table is changed
// source: https://wiki.osdev.org/Paging
static inline void flush_tlb(uint32_t addr) {
    asm volatile("invlpg (%0)" ::"r" (addr) : "memory");
}

// Switch page directory
// When page directory entries have been changed, can switch to oneself to flush the cache
inline void switch_page_directory(uint32_t physical_addr) {
    asm volatile("mov %0, %%cr3": : "r"(physical_addr));
}

static void page_fault_callback(trapframe* regs) {
    UNUSED_ARG(regs);
    void* vaddr; // get the address causing this page fault
    asm volatile("mov %%cr2, %0": "=r"(vaddr));
    
    // detect stack overflow
    proc* p = curr_proc();
    if(p->kernel_stack && vaddr >= p->kernel_stack - PAGE_SIZE && vaddr < p->kernel_stack) {
        printf("KERNEL PANIC: PAGE FAULT (kernel stack overflow)!\n", vaddr);
    } else if(p->user_stack && vaddr >= p->user_stack - PAGE_SIZE && vaddr < p->user_stack) {
        printf("KERNEL PANIC: PAGE FAULT (user stack overflow)!\n", vaddr);
    } else {
        printf("KERNEL PANIC: PAGE FAULT!\n", vaddr);
    }
    printf("  ADDR    0x%x \n  SBRK    0x%x \n  USTACK  0x%x \n  KSTACKB 0x%x\n  KSTACKE 0x%x\n", 
            vaddr, p->size, p->user_stack - PAGE_SIZE, p->kernel_stack - PAGE_SIZE, p->kernel_stack + N_KERNEL_STACK_PAGE_SIZE*PAGE_SIZE);
    
    while (1);
}

static void install_page_fault_handler() {
    register_interrupt_handler(PAGE_FAULT_INTERRUPT, page_fault_callback);
}

static bool is_curr_page_dir(pde* page_dir)
{
    return page_dir[PAGE_DIR_SIZE-1].page_table_frame == PAGE_DIR_PTR[PAGE_DIR_SIZE-1].page_table_frame;
}

pde* curr_page_dir()
{
    return PAGE_DIR_PTR;
}

static void alloc_page_table(pde* page_dir, uint page_dir_idx)
{
    PANIC_ASSERT(!page_dir[page_dir_idx].present);

    uint32_t  page_table_frame = first_free_frame();
    set_frame(page_table_frame);

    pde page_dir_entry = { .present = 1, .rw = 1, .user = 1, .page_table_frame = page_table_frame };
    page_dir[page_dir_idx] = page_dir_entry;
    if(is_curr_page_dir(page_dir)) {
        switch_page_directory(PAGE_DIR_PHYSICAL_ADDR);  // flush
    }
}

static page_t* get_page_table(pde* page_dir, uint32_t page_dir_idx, bool allow_alloc)
{
    PANIC_ASSERT(page_dir_idx < PAGE_DIR_SIZE);
    PANIC_ASSERT(page_dir[page_dir_idx].present || allow_alloc);

    bool new_page_table = false;
    if(!page_dir[page_dir_idx].present && allow_alloc) {
        alloc_page_table(page_dir, page_dir_idx);
        new_page_table = true;
    }

    page_t* page_table;
    if(is_curr_page_dir(page_dir)) {
        page_table = PAGE_TABLE_PTR(page_dir_idx);
    } else {
        uint32_t page_table_frame = page_dir[page_dir_idx].page_table_frame;
        uint32_t page_table_page_index = find_contiguous_free_pages(curr_page_dir(), 1, true);
        map_pages_at(curr_page_dir(), page_table_page_index, 1, &page_table_frame, true, true, false);
        page_table = (page_t*) VADDR_FROM_PAGE_INDEX(page_table_page_index);
    }

    if(new_page_table) {
        memset(page_table, 0, sizeof(*page_table)*PAGE_TABLE_SIZE);
    }

    return page_table;
}

// unmap the page table returned by get_page_table
static void return_page_table(pde* page_dir, page_t* page_table)
{
    if(!is_curr_page_dir(page_dir)) {
        unmap_pages_from(curr_page_dir(), PAGE_INDEX_FROM_VADDR((uint32_t) page_table), 1, false, false);
    }
}

// Find contiguous pages that have not been mapped
// If is for kernel, search vaddr space after KERNEL_VIRTUAL_END
// return: the first page index of the contiguous unmapped virtual memory space
static uint32_t find_contiguous_free_pages(pde* page_dir, size_t page_count, bool is_kernel) {
    uint32_t page_dir_idx_0, page_dir_idx_max;
    uint32_t kernel_page_dir_idx = PAGE_INDEX_FROM_VADDR((uint32_t) MAP_MEM_PA_ZERO_TO) / PAGE_TABLE_SIZE;
    if(is_kernel) {
        page_dir_idx_0 = PAGE_INDEX_FROM_VADDR((uint32_t) KERNEL_VIRTUAL_END) / PAGE_TABLE_SIZE;
        page_dir_idx_max = PAGE_DIR_SIZE;
    } else {
        page_dir_idx_0 = 0;
        page_dir_idx_max = kernel_page_dir_idx;
    }
    uint32_t contiguous_page_count = 0;
    for (uint32_t page_dir_idx = page_dir_idx_0; page_dir_idx < page_dir_idx_max; page_dir_idx++) {
        if (page_dir[page_dir_idx].present == 0) {
            if (contiguous_page_count + PAGE_TABLE_SIZE >= page_count) {
                return page_dir_idx * PAGE_TABLE_SIZE - contiguous_page_count;
            }
            contiguous_page_count += PAGE_TABLE_SIZE;
            continue;
        }
        uint32_t page_table_idx;
        if(is_kernel && page_dir_idx==page_dir_idx_0) {
            page_table_idx = PAGE_INDEX_FROM_VADDR((uint32_t) KERNEL_VIRTUAL_END) % PAGE_TABLE_SIZE;
        } else {
            page_table_idx = 0;
        }
        for (; page_table_idx < PAGE_TABLE_SIZE; page_table_idx++) {
            page_t* page_table = get_page_table(page_dir, page_dir_idx, false);
            if (page_table[page_table_idx].present == 0) {
                if (contiguous_page_count + 1 >= page_count) {
                    return_page_table(page_dir, page_table);
                    return page_dir_idx * PAGE_TABLE_SIZE + page_table_idx - contiguous_page_count;
                }
                contiguous_page_count++;
            } else {
                contiguous_page_count = 0;
            }
            return_page_table(page_dir, page_table);
        }
    }

    PANIC("Failed to find a contiguous VA");

}

// Map or allocate pages
//@param frames frame index arrary of length page_count, map to these frames. If NULL, allocate new frames
//      If consecutive_frame is true and frames is not null, will map *frames, *frames + 1, *frames + 2 ...
//@return number of frames mapped
uint map_pages_at(pde* page_dir, uint page_index, uint page_count, uint32_t* frames,  bool is_kernel, bool is_writeable, bool consecutive_frame)
{
    if(page_count == 0) {
        return 0;
    }

    size_t page_allocated = 0;

    uint page_dir_idx = page_index / PAGE_TABLE_SIZE;
    uint page_table_idx = page_index % PAGE_TABLE_SIZE;
    uint kernel_page_dir_idx = PAGE_INDEX_FROM_VADDR((uint) MAP_MEM_PA_ZERO_TO) / PAGE_TABLE_SIZE;

    PANIC_ASSERT(page_table_idx < PAGE_TABLE_SIZE);
    PANIC_ASSERT((page_dir_idx < kernel_page_dir_idx) ^ is_kernel);
    page_t* page_table = get_page_table(page_dir, page_dir_idx, true);
    
    uint frame_index;
    if(frames == NULL && consecutive_frame) {
        frame_index = n_free_frames(page_count);
    }
    if(frames != NULL && consecutive_frame) {
        frame_index = *frames;
    }

    while(page_allocated < page_count) {
        page_t old_pte = page_table[page_table_idx];

        // Make sure the page hasn't been mapped to any physical memory
        PANIC_ASSERT(!old_pte.present);

        if(frames == NULL) {
            if(!consecutive_frame) {
                frame_index = first_free_frame();
            }
                set_frame(frame_index);
        } else {
            if(!consecutive_frame) {
                frame_index = *frames++;
            }
            PANIC_ASSERT(test_frame(frame_index));
        }
        
        page_t new_pte = { .present = 1, .user = !is_kernel, .rw = is_writeable, .frame = frame_index };
        page_table[page_table_idx] = new_pte;

        if(is_curr_page_dir(page_dir)) {
            flush_tlb(VADDR_FROM_PAGE_INDEX(page_table_idx + page_dir_idx*PAGE_TABLE_SIZE));
            // printf("Page Mapped: PD[%d]:PT[%d]:Frame[0x%x]\n", page_dir_idx, page_table_idx, frame_index);
        } else {
            // printf("Foreign Page Mapped: PD[%d]:PT[%d]:Frame[0x%x]\n", page_dir_idx, page_table_idx, frame_index);
        }

        page_allocated++;
        if(consecutive_frame) {
            frame_index++;
        }

        page_table_idx++;
        if(page_table_idx >= PAGE_TABLE_SIZE) {
            return_page_table(page_dir, page_table);
            page_table_idx = 0;
            page_dir_idx++;
            PANIC_ASSERT(page_dir_idx < PAGE_DIR_SIZE);
            page_table = get_page_table(page_dir, page_dir_idx, true);
        }
    }

    return page_allocated;
}

// Unmap/Deallocate pages
//@param free_frame if true, deallocate physical frames, otherwise unmap only
//@param skip_unmapped if false, kernel panic if trying to unmap page not present
//@return number of pages unmapped
static uint unmap_pages_from(pde* page_dir, uint page_index, uint page_count, bool free_frame, bool skip_unmapped)
{
    if(page_count == 0) {
        return 0;
    }
    
    size_t page_deallocated = 0;

    uint page_dir_idx = page_index / PAGE_TABLE_SIZE;
    uint page_table_idx = page_index % PAGE_TABLE_SIZE;

    PANIC_ASSERT(page_dir_idx < PAGE_DIR_SIZE);
    page_t* page_table = get_page_table(page_dir, page_dir_idx, true);

    while(page_deallocated < page_count) {

        PANIC_ASSERT(skip_unmapped || page_table[page_table_idx].present);

        if(page_table[page_table_idx].present) {
            if(free_frame) {
                clear_frame(page_table[page_table_idx].frame);
            }
            memset(&page_table[page_table_idx], 0, sizeof(*page_table));

            if(is_curr_page_dir(page_dir)) {
                flush_tlb(VADDR_FROM_PAGE_INDEX(page_table_idx + page_dir_idx*PAGE_TABLE_SIZE));
                // printf("Page Unmapped: PD[%d]:PT[%d]:Frame[0x%x]\n", page_dir_idx, page_table_idx, frame_index);
            } else {
                // printf("Foreign Page Unmapped: PD[%d]:PT[%d]:Frame[0x%x]\n", page_dir_idx, page_table_idx, frame_index);
            }
        }

        page_deallocated++;

        page_table_idx++;
        if(page_table_idx >= PAGE_TABLE_SIZE) {
            return_page_table(page_dir, page_table);
            page_table_idx = 0;
            page_dir_idx++;
            PANIC_ASSERT(page_dir_idx < PAGE_DIR_SIZE);
            page_table = get_page_table(page_dir, page_dir_idx, true);
        }
    }

    return page_deallocated;
}

// Link two virtual address space pages between page dirs by with the same physical memory space
// Find a continuous virtual space in pd_target and map to the physical frames under [vaddr,vaddr+size-1] vaddr space in pd_source
// If the virtual space in pd_source is not mapped, allocate frame & map before linking
//@return vaddr of beginning of the linked space in pd_target
uint32_t link_pages_between(pde* pd_source, uint32_t vaddr, uint32_t size, pde* pd_target, bool alloc_source_rw, bool target_rw)
{
    PANIC_ASSERT(pd_source != pd_target);
    
    // do not allow linking to kernel space (kernel shall always be linked already)
    if(!(vaddr < (uint32_t) MAP_MEM_PA_ZERO_TO && vaddr + size - 1 < (uint32_t) MAP_MEM_PA_ZERO_TO)) {
        PANIC_ASSERT(vaddr < (uint32_t) MAP_MEM_PA_ZERO_TO && vaddr + size - 1 < (uint32_t) MAP_MEM_PA_ZERO_TO);
    }
    
    uint page_idx_source = PAGE_INDEX_FROM_VADDR(vaddr);
    uint offset = vaddr - VADDR_FROM_PAGE_INDEX(page_idx_source);
    
    uint page_count = PAGE_COUNT_FROM_BYTES(offset + size);
    uint page_idx_target = find_contiguous_free_pages(pd_target, page_count, false);
    
    if(page_count == 0) {
        return 0;
    }

    uint dir_idx_source = page_idx_source / PAGE_TABLE_SIZE;
    uint table_idx_source = page_idx_source % PAGE_TABLE_SIZE;
    uint dir_idx_target = page_idx_target / PAGE_TABLE_SIZE;
    uint table_idx_target = page_idx_target % PAGE_TABLE_SIZE;

    size_t page_linked = 0;

    PANIC_ASSERT(dir_idx_source < PAGE_DIR_SIZE);
    PANIC_ASSERT(dir_idx_target < PAGE_DIR_SIZE);
    page_t* table_source = get_page_table(pd_source, dir_idx_source, true);
    page_t* table_target = get_page_table(pd_target, dir_idx_target, true);

    while(page_linked < page_count) {
        page_t* pte_source = &table_source[table_idx_source];

        uint frame_index;
        if(!pte_source->present) {
            frame_index = first_free_frame();
            set_frame(frame_index);
            *pte_source = (page_t) {.present = 1, .user = true, .rw = alloc_source_rw, .frame = frame_index};
            if(is_curr_page_dir(pd_source)) {
                flush_tlb(VADDR_FROM_PAGE_INDEX(table_idx_source + dir_idx_source*PAGE_TABLE_SIZE));
            }
        } else {
            frame_index = pte_source->frame;
        }
        
        page_t* pte_target = &table_target[table_idx_target];

        PANIC_ASSERT(!pte_target->present);

        *pte_target = (page_t) {.present = 1, .user = true, .rw = target_rw, .frame = frame_index};

        if(is_curr_page_dir(pd_target)) {
            flush_tlb(VADDR_FROM_PAGE_INDEX(table_idx_target + dir_idx_target*PAGE_TABLE_SIZE));
        }

        page_linked++;

        table_idx_source++;
        if(table_idx_source >= PAGE_TABLE_SIZE) {
            return_page_table(pd_source, table_source);
            table_idx_source = 0;
            dir_idx_source++;
            PANIC_ASSERT(dir_idx_source < PAGE_DIR_SIZE);
            table_source = get_page_table(pd_source, dir_idx_source, true);
        }

        table_idx_target++;
        if(table_idx_target >= PAGE_TABLE_SIZE) {
            return_page_table(pd_target, table_target);
            table_idx_target = 0;
            dir_idx_target++;
            PANIC_ASSERT(dir_idx_target < PAGE_DIR_SIZE);
            table_target = get_page_table(pd_target, dir_idx_target, true);
        }
    }
    
    return VADDR_FROM_PAGE_INDEX(page_idx_target) + offset;
}

uint32_t change_page_rw_attr(pde* page_dir, uint32_t page_index, bool is_writeable)
{
    uint32_t page_dir_idx = page_index / PAGE_TABLE_SIZE;
    uint32_t page_table_idx = page_index % PAGE_TABLE_SIZE;
    
    PANIC_ASSERT(page_dir[page_dir_idx].present);
    page_t* page_table = get_page_table(page_dir, page_dir_idx, false);
    
    page_table[page_table_idx].rw = is_writeable;

    return_page_table(page_dir, page_table);

    if(is_curr_page_dir(page_dir)) {
        flush_tlb(VADDR_FROM_PAGE_INDEX(page_index));
    }

    return VADDR_FROM_PAGE_INDEX(page_index);
}

uint32_t vaddr2paddr(pde* page_dir, uint32_t vaddr)
{
    uint32_t page_index = PAGE_INDEX_FROM_VADDR(vaddr);
    uint32_t page_dir_idx = page_index / PAGE_TABLE_SIZE;
    uint32_t page_table_idx = page_index % PAGE_TABLE_SIZE;
    PANIC_ASSERT(page_dir[page_dir_idx].present);
    page_t* page_table = get_page_table(page_dir, page_dir_idx, false);
    page_t* page = &page_table[page_table_idx];
    PANIC_ASSERT(page->present);
    uint32_t paddr = (page->frame << 12) + (vaddr & 0xFFF);
    return_page_table(page_dir, page_table);
    return paddr;
}

void dealloc_pages(pde* page_dir, uint32_t page_index, size_t page_count) {
    unmap_pages_from(page_dir, page_index, page_count, true, false);
}

// allocate frames for pages starting at vaddr, panic if already mapped
// return: starting vaddr of the allocated pages 
uint32_t alloc_pages_at(pde* page_dir, uint32_t page_index, size_t page_count, bool is_kernel, bool is_writeable)
{
    if(page_count == 0) {
        return 0;
    }
    map_pages_at(page_dir, page_index, page_count, NULL, is_kernel, is_writeable, false);
    return VADDR_FROM_PAGE_INDEX(page_index);
}

// return: starting vaddr of the allocated pages 
uint32_t alloc_pages(pde* page_dir, size_t page_count, bool is_kernel, bool is_writeable) {
    if (page_count == 0) {
        return 0;
    }
    uint32_t page_index = find_contiguous_free_pages(page_dir, page_count, is_kernel);
    map_pages_at(page_dir, page_index, page_count, NULL, is_kernel, is_writeable, false);
    return VADDR_FROM_PAGE_INDEX(page_index);
}

uint32_t alloc_pages_consecutive_frames(pde* page_dir, size_t page_count, bool is_writeable, uint32_t* physical_addr) {
    if (page_count == 0) {
        return 0;
    }
    uint32_t page_index = find_contiguous_free_pages(page_dir, page_count, true);
    map_pages_at(page_dir, page_index, page_count, NULL, true, is_writeable, true);
    uint32_t vaddr = VADDR_FROM_PAGE_INDEX(page_index);
    if(physical_addr != NULL) {
        *physical_addr = vaddr2paddr(page_dir, vaddr);
    }
    return vaddr;
}

bool is_vaddr_accessible(pde* page_dir, uint32_t vaddr, bool is_from_kernel_code, bool is_writing) {
    UNUSED_ARG(is_from_kernel_code);

    uint32_t page_index = PAGE_INDEX_FROM_VADDR(vaddr);
    uint32_t page_dir_idx = page_index / PAGE_TABLE_SIZE;
    uint32_t page_table_idx = page_index % PAGE_TABLE_SIZE;
    if (!page_dir[page_dir_idx].present) {
        return false;
    }
    page_t* page_table = get_page_table(page_dir, page_dir_idx, false);

    bool accessible;
    if (!page_table[page_table_idx].present) {
        accessible = false;
    }
    // commenting out this section because we have set CR0 WP bit
    // else if (is_from_kernel_code) {
    //     // Kernel can do anything, assuming we are not in write protected mode
    //     // "The WP bit in CR0 determines if this is only applied to userland, 
    //     //   always giving the kernel write access (the default) or both userland and the kernel 
    //     //   (see Intel Manuals 3A 2-20)"
    //     accessible = true;
    // }
    else if (page_dir[page_dir_idx].rw < is_writing) {
        accessible = false;
    }
    else if (!page_dir[page_dir_idx].user) {
        accessible = false;
    } 
    else if (page_table[page_table_idx].rw < is_writing) {
        accessible = false;
    }
    else if(!page_table[page_table_idx].user) {
        accessible = false;
    }
    else {
        accessible = true;
    }

    return_page_table(page_dir, page_table);

    return accessible;
}

// Set current TSS segment
void set_tss(uint32_t kernel_stack_esp)
{
    cpu* cpu = curr_cpu();
    cpu->gdt[SEG_TSS] = TSS_SEG(STS_T32A, &cpu->ts, sizeof(cpu->ts)-1, DPL_KERNEL);
    cpu->ts.ss0 = SEG_SELECTOR(SEG_KDATA, DPL_KERNEL);
    cpu->ts.esp0 = kernel_stack_esp;
    // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
    // forbids I/O instructions (e.g., inb and outb) from user space
    cpu->ts.iomb = (uint16_t) 0xFFFF;
    ltr(SEG_SELECTOR(SEG_TSS, DPL_KERNEL));
}

// copy all kernel space page dir entries from current dir to page_dir 
void copy_kernel_space_mapping(pde* page_dir)
{
    uint32_t kernel_page_dir_idx = PAGE_INDEX_FROM_VADDR((uint32_t) MAP_MEM_PA_ZERO_TO) / PAGE_TABLE_SIZE;
    pde* current_page_dir = curr_page_dir();
    for(int i=kernel_page_dir_idx;i<PAGE_DIR_SIZE;i++) {
        page_dir[i] = current_page_dir[i];
    }
    // maintain page dir recursion
    page_dir[PAGE_DIR_SIZE-1].page_table_frame = vaddr2paddr(curr_page_dir(), (uint32_t) page_dir) >> 12;
}

// unmap (not dealloc) pages underlying vaddr to vaddr+size
void unmap_pages(pde* page_dir, uint32_t vaddr, uint32_t size)
{
    uint32_t page_idx = PAGE_INDEX_FROM_VADDR(vaddr);
    uint32_t offset = vaddr - VADDR_FROM_PAGE_INDEX(page_idx);
    uint32_t n_page = PAGE_COUNT_FROM_BYTES(offset + size);

    unmap_pages_from(page_dir, page_idx, n_page, false, false);
}

// unmap/free all user space pages, free all underlying frames
void free_user_space(pde* page_dir)
{
    uint32_t kernel_page_dir_idx = PAGE_INDEX_FROM_VADDR((uint32_t) MAP_MEM_PA_ZERO_TO) / PAGE_TABLE_SIZE;
    unmap_pages_from(page_dir, 0, kernel_page_dir_idx, true, true);
}

pde* alloc_page_dir()
{
    pde* page_dir = (pde*) alloc_pages(curr_page_dir(), 1, true, true);
    memset(page_dir, 0, sizeof(pde)*PAGE_DIR_SIZE);
    return page_dir;
}

pde* copy_user_space(pde* page_dir)
{
    pde* new_page_dir = alloc_page_dir();
    uint32_t kernel_page_dir_idx = PAGE_INDEX_FROM_VADDR((uint32_t) MAP_MEM_PA_ZERO_TO) / PAGE_TABLE_SIZE;
    for(uint32_t i=0;i<kernel_page_dir_idx;i++) {
        if(page_dir[i].present) {
            // copy page dir entry
            new_page_dir[i] = page_dir[i];
            // allocate one new page table
            uint32_t page_table_vaddr = alloc_pages(curr_page_dir(), 1, true, true);
            uint32_t page_table_paddr = vaddr2paddr(curr_page_dir(), page_table_vaddr);
            new_page_dir[i].page_table_frame = FRAME_INDEX_FROM_ADDR(page_table_paddr);

            page_t* new_page_table =  get_page_table(new_page_dir, i, true);
            page_t* page_table = get_page_table(page_dir, i, false);
            
            for(int j=0;j<PAGE_TABLE_SIZE;j++) {
                if(page_table[j].present) {
                    // copy frame content
                    uint32_t page_idx = i*PAGE_TABLE_SIZE + j;
                    uint32_t src;
                    if(is_curr_page_dir(page_dir)) {
                        src = VADDR_FROM_PAGE_INDEX(page_idx);
                    } else {
                        src = link_pages_between(page_dir, VADDR_FROM_PAGE_INDEX(page_idx), PAGE_SIZE, curr_page_dir(), false, false);
                    }
                    uint32_t dst = link_pages_between(new_page_dir, VADDR_FROM_PAGE_INDEX(page_idx), PAGE_SIZE, curr_page_dir(), page_table[j].rw, true);
                    memmove((char*) dst, (char*) src, PAGE_SIZE);
                    unmap_pages_from(curr_page_dir(), PAGE_INDEX_FROM_VADDR(dst), 1, false, false);
                    if(!is_curr_page_dir(page_dir)) {
                        unmap_pages_from(curr_page_dir(), PAGE_INDEX_FROM_VADDR(src), 1, false, false);
                    }
                    // copy page table entry
                    uint32_t new_frame = new_page_table[j].frame;
                    new_page_table[j] = page_table[j];
                    new_page_table[j].frame = new_frame;
                }
            }
            return_page_table(page_dir, page_table);
            return_page_table(new_page_dir, new_page_table);
        }
    }
    return new_page_dir;
}

void initialize_paging()
{
    init_gdt();
    install_page_fault_handler();

    printf("Boot page dir physical addr: 0x%x\n", PAGE_DIR_PHYSICAL_ADDR);
    pde page_dir_entry_0 = curr_page_dir()[0xC0000000 >> 22];
    printf("Boot page table physical addr: 0x%x\n", page_dir_entry_0.page_table_frame << 12);
    page_t page_table_entry_0 = PAGE_TABLE_PTR(0xC0000000 >> 22)[0];
    printf("Boot page table entry 0 point to physical addr: 0x%x\n", page_table_entry_0.frame << 12);

    pde* curr_dir = curr_page_dir();
    printf("vaddr2paddr: page_dir is mapped to: %u, PHY=%u\n", vaddr2paddr(curr_dir, (uint32_t) curr_dir), PAGE_DIR_PHYSICAL_ADDR);
    PANIC_ASSERT((uint32_t) PAGE_DIR_PHYSICAL_ADDR == vaddr2paddr(curr_page_dir(), (uint32_t) curr_page_dir()));

}
