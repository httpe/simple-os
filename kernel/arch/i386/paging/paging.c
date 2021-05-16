#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <kernel/panic.h>
#include <common.h>
#include <kernel/memory_bitmap.h>
#include <kernel/paging.h>

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
static uint32_t map_page(pde* page_dir, uint32_t page_index, uint32_t frame_index, bool is_kernel, bool is_writeable);
static uint32_t unmap_page(pde* page_dir, uint32_t page_index);


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

// map page table's physical frame to a page and return as pointer, need to unmap by return_page_table
static page_t* get_page_table(pde* page_dir, uint32_t page_dir_idx)
{
    PANIC_ASSERT(page_dir[page_dir_idx].present);
    page_t* page_table;
    if(is_curr_page_dir(page_dir)) {
        page_table = PAGE_TABLE_PTR(page_dir_idx);
    } else {
        uint32_t page_table_frame = page_dir[page_dir_idx].page_table_frame;
        uint32_t page_table_page_index = find_contiguous_free_pages(curr_page_dir(), 1, true);
        page_table = (page_t*) map_page(curr_page_dir(), page_table_page_index, page_table_frame, true, true);
    }

    return page_table;
}

// unmap the page table returned by get_page_table
static void return_page_table(pde* page_dir, page_t* page_table)
{
    if(!is_curr_page_dir(page_dir)) {
        unmap_page(curr_page_dir(), PAGE_INDEX_FROM_VADDR((uint32_t) page_table));
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
            page_t* page_table = get_page_table(page_dir, page_dir_idx);
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

// return: physical frame index unmapped
static uint32_t unmap_page(pde* page_dir, uint32_t page_index)
{
    uint32_t vaddr = VADDR_FROM_PAGE_INDEX(page_index);
    uint32_t page_dir_idx = page_index / PAGE_TABLE_SIZE;
    uint32_t page_table_idx = page_index % PAGE_TABLE_SIZE;

    page_t* page_table = get_page_table(page_dir, page_dir_idx);

    page_t* page = &page_table[page_table_idx];
    PANIC_ASSERT(page->present);

    page->present = 0;
    uint32_t page_frame = page->frame;
    
    return_page_table(page_dir, page_table);

    if(is_curr_page_dir(page_dir)) {
        flush_tlb(vaddr);   // flush
        // printf("Page unmapped: PD[%d]:PT[%d]:Frame[0x%x]\n", page_dir_idx, page_table_idx, page->frame);
    } else {
        // printf("Foreign Page unmapped: PD[%d]:PT[%d]:Frame[0x%x]\n", page_dir_idx, page_table_idx, page->frame);
    }

    return page_frame;
}

// return: mapped vaddr
static uint32_t map_page(pde* page_dir, uint32_t page_index, uint32_t frame_index, bool is_kernel, bool is_writeable)
{
    uint32_t page_dir_idx = page_index / PAGE_TABLE_SIZE;
    uint32_t page_table_idx = page_index % PAGE_TABLE_SIZE;

    uint32_t kernel_page_dir_idx = PAGE_INDEX_FROM_VADDR((uint32_t) MAP_MEM_PA_ZERO_TO) / PAGE_TABLE_SIZE;
    PANIC_ASSERT((page_dir_idx < kernel_page_dir_idx) ^ is_kernel);

    set_frame(frame_index);                    // ensure is frame is marked used

    page_t* page_table;
    if (!page_dir[page_dir_idx].present) {
        // Page table not allocated, create it first
        uint32_t  page_table_frame = first_free_frame(); // idx is now the index of the first free frame.
        set_frame(page_table_frame);                    // this frame is now ours!
        pde page_dir_entry = { .present = 1, .rw = 1, .user = 1, .page_table_frame = page_table_frame };
        page_dir[page_dir_idx] = page_dir_entry;
        if(is_curr_page_dir(page_dir)) {
            switch_page_directory(PAGE_DIR_PHYSICAL_ADDR);  // flush
        }
        // new_page_table = true;
        page_table = get_page_table(page_dir, page_dir_idx);
        memset(page_table, 0, sizeof(page_t) * PAGE_TABLE_SIZE);
    } else {
        page_table = get_page_table(page_dir, page_dir_idx);
    }

    page_t old_page = page_table[page_table_idx];
    PANIC_ASSERT(!old_page.present);
    // Not present means it hasn't been mapped to physical space
    page_t page = { .present = 1, .user = !is_kernel, .rw = is_writeable, .frame = frame_index };
    page_table[page_table_idx] = page;

    return_page_table(page_dir, page_table);

    if(is_curr_page_dir(page_dir)) {
        flush_tlb(VADDR_FROM_PAGE_INDEX(page_index));
        // printf("Page frame mapped: PD[%d]:PT[%d]:Frame[0x%x]\n", page_dir_idx, page_table_idx, frame_index);
    } else {
        // printf("Foreign Page frame mapped: PD[%d]:PT[%d]:Frame[0x%x]\n", page_dir_idx, page_table_idx, frame_index);
    }

    return VADDR_FROM_PAGE_INDEX(page_index);

}

uint32_t change_page_rw_attr(pde* page_dir, uint32_t page_index, bool is_writeable)
{
    uint32_t page_dir_idx = page_index / PAGE_TABLE_SIZE;
    uint32_t page_table_idx = page_index % PAGE_TABLE_SIZE;
    
    PANIC_ASSERT(page_dir[page_dir_idx].present);
    page_t* page_table = get_page_table(page_dir, page_dir_idx);
    
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
    page_t* page_table = get_page_table(page_dir, page_dir_idx);
    page_t* page = &page_table[page_table_idx];
    PANIC_ASSERT(page->present);
    uint32_t paddr = (page->frame << 12) + (vaddr & 0xFFF);
    return_page_table(page_dir, page_table);
    return paddr;
}

// Unmapped a page, and then free its mapped frame
static void free_frame(pde* page_dir, uint32_t page_index) {
    uint32_t vaddr = VADDR_FROM_PAGE_INDEX(page_index);
    // Fill with garbage to crash dangling reference/pointer earlier
    memset((char*)vaddr, 1, PAGE_SIZE);
    uint32_t frame_index = unmap_page(page_dir, page_index);
    clear_frame(frame_index);
}

// Allocate physical space for a page (must not be already mapped)
// return: allocated physical frame index
static uint32_t alloc_frame(pde* page_dir, uint32_t page_index, bool is_kernel, bool is_writeable) {
    uint32_t frame_index = first_free_frame(); // idx is now the index of the first free frame.
    map_page(page_dir, page_index, frame_index, is_kernel, is_writeable);
    return frame_index;
}

void dealloc_pages(pde* page_dir, uint32_t page_index, size_t page_count) {
    if (page_count == 0) {
        return;
    }
    
    for (uint32_t idx = page_index; idx < page_index + page_count; idx++) {
        free_frame(page_dir, idx);
    }
}

// allocate frames for pages starting at vaddr, panic if already mapped
// return: starting vaddr of the allocated pages 
uint32_t alloc_pages_at(pde* page_dir, uint32_t page_index, size_t page_count, bool is_kernel, bool is_writeable)
{
    if(page_count == 0) {
        return 0;
    }

    for (uint32_t idx = page_index; idx < page_index + page_count; idx++) {
        alloc_frame(page_dir, idx, is_kernel, is_writeable);
    }

    return VADDR_FROM_PAGE_INDEX(page_index);
}

// return: starting vaddr of the allocated pages 
uint32_t alloc_pages(pde* page_dir, size_t page_count, bool is_kernel, bool is_writeable) {
    if (page_count == 0) {
        return 0;
    }

    uint32_t page_index = find_contiguous_free_pages(page_dir, page_count, is_kernel);
    for (uint32_t idx = page_index; idx < page_index + page_count; idx++) {
        alloc_frame(page_dir, idx, is_kernel, is_writeable);
    }

    return VADDR_FROM_PAGE_INDEX(page_index);
}

bool is_vaddr_accessible(pde* page_dir, uint32_t vaddr, bool is_from_kernel_code, bool is_writing) {
    UNUSED_ARG(is_from_kernel_code);

    uint32_t page_index = PAGE_INDEX_FROM_VADDR(vaddr);
    uint32_t page_dir_idx = page_index / PAGE_TABLE_SIZE;
    uint32_t page_table_idx = page_index % PAGE_TABLE_SIZE;
    if (!page_dir[page_dir_idx].present) {
        return false;
    }
    page_t* page_table = get_page_table(page_dir, page_dir_idx);

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

// make the vaddr[size] space of from_page_dir available in to_page_dir
// link them to the same physical memory
// if vaddr[size] space is not mapped in from_page_dir, allocate it first
// shall only be used for user space memory area since kernel space is always linked
// return: vaddr of to_page_dir to access the space
uint32_t link_pages(pde* from_page_dir, uint32_t vaddr, uint32_t size, pde* to_page_dir, bool alloc_writable_page)
{
    // do not allow linking to kernel space
    PANIC_ASSERT(vaddr < (uint32_t) MAP_MEM_PA_ZERO_TO || vaddr + size < (uint32_t) MAP_MEM_PA_ZERO_TO);

    uint32_t page_idx_foreign = PAGE_INDEX_FROM_VADDR(vaddr);
    uint32_t offset = vaddr - VADDR_FROM_PAGE_INDEX(page_idx_foreign);
    
    uint32_t n_page = PAGE_COUNT_FROM_BYTES(offset + size);
    uint32_t page_idx_curr = find_contiguous_free_pages(to_page_dir, n_page, true);
    for(uint32_t i=0; i<n_page;i++) {
        uint32_t frame_idx;
        uint32_t page_idx = page_idx_foreign + i;
        uint32_t vaddr_foreign =  VADDR_FROM_PAGE_INDEX(page_idx);
        if(is_vaddr_accessible(from_page_dir, vaddr_foreign, false, alloc_writable_page)) {
            uint32_t paddr = vaddr2paddr(from_page_dir, vaddr_foreign);
            frame_idx = FRAME_INDEX_FROM_ADDR(paddr);
        } else {
            // make sure the frame is not allocated already
            //   and fail the test above because of write protection
            assert(!is_vaddr_accessible(from_page_dir, vaddr_foreign, false, false));

            frame_idx = alloc_frame(from_page_dir, page_idx, false, alloc_writable_page);
        }
        // for mapping in to_page_dir, always allow read & write
        map_page(to_page_dir, page_idx_curr+i, frame_idx, true, true);
    }

    return VADDR_FROM_PAGE_INDEX(page_idx_curr) + offset;
}

// unmap (not dealloc) pages underlying vaddr to vaddr+size
void unmap_pages(pde* page_dir, uint32_t vaddr, uint32_t size)
{
    uint32_t page_idx = PAGE_INDEX_FROM_VADDR(vaddr);
    uint32_t offset = vaddr - VADDR_FROM_PAGE_INDEX(page_idx);
    uint32_t n_page = PAGE_COUNT_FROM_BYTES(offset + size);
    for(uint32_t i=0; i<n_page;i++) {
        unmap_page(page_dir, page_idx + i);
    }
}

// unmap/free all user space pages, free all underlying frames
void free_user_space(pde* page_dir)
{
    uint32_t kernel_page_dir_idx = PAGE_INDEX_FROM_VADDR((uint32_t) MAP_MEM_PA_ZERO_TO) / PAGE_TABLE_SIZE;
    for(uint32_t i=0;i<kernel_page_dir_idx;i++) {
        if(page_dir[i].present) {
            page_t* page_table = get_page_table(page_dir, i);
            for(int j=0;j<PAGE_TABLE_SIZE;j++) {
                if(page_table[j].present) {
                    page_table[j].present = 0;
                    clear_frame(page_table[j].frame);
                }
            }
            page_dir[i].present = 0;
            clear_frame(page_dir[i].page_table_frame);
            return_page_table(page_dir, page_table);
        }
    }
    if(is_curr_page_dir(page_dir)) {
        switch_page_directory(PAGE_DIR_PHYSICAL_ADDR);
    }
    // dealloc_pages(curr_page_dir(), (uint32_t) page_dir, 1);
}

pde* copy_user_space(pde* page_dir)
{
    pde* new_page_dir = (pde*) alloc_pages(curr_page_dir(), 1, true, true);
    memset(new_page_dir, 0, sizeof(*new_page_dir)*PAGE_DIR_SIZE);
    uint32_t kernel_page_dir_idx = PAGE_INDEX_FROM_VADDR((uint32_t) MAP_MEM_PA_ZERO_TO) / PAGE_TABLE_SIZE;
    for(uint32_t i=0;i<kernel_page_dir_idx;i++) {
        if(page_dir[i].present) {
            // copy page dir entry
            new_page_dir[i] = page_dir[i];
            // allocate one new page table
            uint32_t page_table_vaddr = alloc_pages(curr_page_dir(), 1, true, true);
            uint32_t page_table_paddr = vaddr2paddr(curr_page_dir(), page_table_vaddr);
            new_page_dir[i].page_table_frame = FRAME_INDEX_FROM_ADDR(page_table_paddr);

            page_t* new_page_table =  get_page_table(new_page_dir, i);
            memset(new_page_table, 0, sizeof(*new_page_table)*PAGE_TABLE_SIZE);
            page_t* page_table = get_page_table(page_dir, i);
            
            for(int j=0;j<PAGE_TABLE_SIZE;j++) {
                if(page_table[j].present) {
                    // copy frame content
                    uint32_t page_idx = i*PAGE_TABLE_SIZE + j;
                    uint32_t src;
                    if(is_curr_page_dir(page_dir)) {
                        src = VADDR_FROM_PAGE_INDEX(page_idx);
                    } else {
                        src = link_pages(page_dir, VADDR_FROM_PAGE_INDEX(page_idx), PAGE_SIZE, curr_page_dir(), false);
                    }
                    uint32_t dst = link_pages(new_page_dir, VADDR_FROM_PAGE_INDEX(page_idx), PAGE_SIZE, curr_page_dir(), page_table[j].rw);
                    memmove((char*) dst, (char*) src, PAGE_SIZE);
                    unmap_page(curr_page_dir(), PAGE_INDEX_FROM_VADDR(dst));
                    if(!is_curr_page_dir(page_dir)) {
                        unmap_page(curr_page_dir(), PAGE_INDEX_FROM_VADDR(src));
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

pde* alloc_page_dir()
{
    // allocate page dir
    pde* page_dir = (pde*) alloc_pages(curr_page_dir(), 1, true, true);
    memset(page_dir, 0, sizeof(pde)*PAGE_DIR_SIZE);
    return page_dir;
}

void initialize_paging() {
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
