#include <stdio.h>
#include <string.h>
#include <kernel/panic.h>
#include <common.h>
#include <kernel/memory_bitmap.h>
#include <kernel/paging.h>

#include <arch/i386/kernel/isr.h>
#include <arch/i386/kernel/segmentation.h>
#include <arch/i386/kernel/paging.h>

extern char MAP_MEM_PA_ZERO_TO[], KERNEL_VIRTUAL_END[];

static segdesc gdt[NSEGS];
static task_state tss;

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
    gdt[SEG_NULL] = (segdesc) {0};
    gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
    gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
    gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
    gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
    // TSS will be setup later, but we tell CPU there will one through size argument of lgdt (NSEGS)
    lgdt(gdt, sizeof(gdt));
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
    uint32_t cr2;
    asm volatile("mov %%cr2, %0": "=r"(cr2));
    printf("KERNEL PANIC: PAGE FAULT! Address: 0x%x", cr2);
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
uint32_t find_contiguous_free_pages(pde* page_dir, size_t page_count, bool is_kernel) {
    uint32_t page_dir_idx_0;
    if(is_kernel) {
        page_dir_idx_0 = PAGE_INDEX_FROM_VADDR((uint32_t) KERNEL_VIRTUAL_END) / PAGE_TABLE_SIZE;
    } else {
        page_dir_idx_0 = 0;
    }
    uint32_t contiguous_page_count = 0;
    for (uint32_t page_dir_idx = page_dir_idx_0; page_dir_idx < PAGE_DIR_SIZE; page_dir_idx++) {
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

    printf("KERNEL PANIC: Find contiguous VA failed\n");
    while (1);

}

// return: physical frame index unmapped
uint32_t unmap_page(pde* page_dir, uint32_t page_index)
{
    uint32_t vaddr = VADDR_FROM_PAGE_INDEX(page_index);
    uint32_t page_dir_idx = page_index / PAGE_TABLE_SIZE;
    uint32_t page_table_idx = page_index % PAGE_TABLE_SIZE;

    page_t* page_table = get_page_table(page_dir, page_dir_idx);

    page_t* page = &page_table[page_table_idx];
    PANIC_ASSERT(page->present);

    page->present = 0;
    uint32_t page_frame = page->frame;
    
    if(is_curr_page_dir(page_dir)) {
        flush_tlb(vaddr);   // flush
        printf("Page unmapped: PD[%d]:PT[%d]:Frame[0x%x]\n", page_dir_idx, page_table_idx, page->frame);
    } else {
        return_page_table(page_dir, page_table);
        printf("Foreign Page unmapped: PD[%d]:PT[%d]:Frame[0x%x]\n", page_dir_idx, page_table_idx, page->frame);
    }

    return page_frame;
}

// return: mapped vaddr
uint32_t map_page(pde* page_dir, uint32_t page_index, uint32_t frame_index, bool is_kernel, bool is_writeable)
{
    uint32_t page_dir_idx = page_index / PAGE_TABLE_SIZE;
    uint32_t page_table_idx = page_index % PAGE_TABLE_SIZE;
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

    if(is_curr_page_dir(page_dir)) {
        flush_tlb(VADDR_FROM_PAGE_INDEX(page_index));
        printf("Page frame mapped: PD[%d]:PT[%d]:Frame[0x%x]\n", page_dir_idx, page_table_idx, frame_index);
    } else {
        return_page_table(page_dir, page_table);
        printf("Foreign Page frame mapped: PD[%d]:PT[%d]:Frame[0x%x]\n", page_dir_idx, page_table_idx, frame_index);
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

void dealloc_pages(pde* page_dir, uint32_t vaddr, size_t page_count) {
    if (page_count == 0) {
        return;
    }

    uint32_t page_index = PAGE_INDEX_FROM_VADDR(vaddr);
    for (uint32_t idx = page_index; idx < page_index + page_count; idx++) {
        free_frame(page_dir, idx);
    }
}

// allocate frames for pages starting at vaddr, panic if already mapped
// return: starting vaddr of the allocated pages 
uint32_t alloc_pages_at(pde* page_dir, uint32_t vaddr, size_t size, bool is_kernel, bool is_writeable)
{
    uint32_t offset = vaddr - VADDR_FROM_PAGE_INDEX(PAGE_INDEX_FROM_VADDR(vaddr));
    uint32_t page_index = PAGE_INDEX_FROM_VADDR(vaddr);
    uint32_t page_count = PAGE_COUNT_FROM_BYTES(offset + size);
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

    uint32_t page_index = find_contiguous_free_pages(curr_page_dir(), page_count, is_kernel);
    for (uint32_t idx = page_index; idx < page_index + page_count; idx++) {
        alloc_frame(page_dir, idx, is_kernel, is_writeable);
    }

    return VADDR_FROM_PAGE_INDEX(page_index);
}

bool is_vaddr_accessible(pde* page_dir, uint32_t vaddr, bool is_from_kernel_code, bool is_writing) {
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
    else if (is_from_kernel_code) {
        // Kernel can do anything, assuming we are not in write protected mode
        // "The WP bit in CR0 determines if this is only applied to userland, 
        //   always giving the kernel write access (the default) or both userland and the kernel 
        //   (see Intel Manuals 3A 2-20)"
        accessible = true;
    }
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
    gdt[SEG_TSS] = TSS_SEG(STS_T32A, &tss, sizeof(tss)-1, DPL_KERNEL);
    tss.ss0 = SEG_SELECTOR(SEG_KDATA, DPL_KERNEL);
    tss.esp0 = kernel_stack_esp;
    // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
    // forbids I/O instructions (e.g., inb and outb) from user space
    tss.iomb = (uint16_t) 0xFFFF;
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
// return: vaddr of to_page_dir to access the space
uint32_t link_pages(pde* from_page_dir, uint32_t vaddr, uint32_t size, pde* to_page_dir, bool alloc_writable_page)
{
    uint32_t page_idx_foreign = PAGE_INDEX_FROM_VADDR(vaddr);
    uint32_t offset = vaddr - VADDR_FROM_PAGE_INDEX(page_idx_foreign);
    
    uint32_t n_page = PAGE_COUNT_FROM_BYTES(offset + size);
    uint32_t page_idx_curr = find_contiguous_free_pages(curr_page_dir(), n_page, true);
    for(uint32_t i=0; i<n_page;i++) {
        uint32_t frame_idx;
        uint32_t page_idx = page_idx_foreign + i;
        uint32_t vaddr_foreign =  VADDR_FROM_PAGE_INDEX(page_idx);
        if(is_vaddr_accessible(from_page_dir, vaddr_foreign, false, alloc_writable_page)) {
            uint32_t paddr = vaddr2paddr(from_page_dir, vaddr_foreign);
            frame_idx = FRAME_INDEX_FROM_ADDR(paddr);
        } else {
            frame_idx = alloc_frame(from_page_dir, page_idx, false, alloc_writable_page);
        }
        map_page(to_page_dir, page_idx_curr+i, frame_idx,true, true);
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

    // uint32_t array_len = 0x9FC00;
    // uint32_t alloc_addr = kmalloc(PAGE_COUNT_FROM_BYTES(array_len), true, true);
    // printf("Allocated an uint32_t[%d] array at virtual address: 0x%x\n", array_len, alloc_addr);
    // uint8_t *array = (uint8_t*) alloc_addr;
    // array[0] = 1;
    // array[array_len-1] = 10;
    // printf("Array[0]=%d; Array[%d]=%d\n", array[0], array_len-1, array[array_len-1]);

    // uint32_t alloc_addr2 = kmalloc(PAGE_COUNT_FROM_BYTES(0x3000), true, true);
    // printf("Allocated second uint32_t[0x3000] array at virtual address: 0x%x\n", alloc_addr2);
    // uint8_t *array2 = (uint8_t*) alloc_addr2;
    // array2[0] = 6;
    // array2[0x3000-1] = 9;
    // printf("Array2[0]=%d; Array2[0x3000-1]=%d\n", array2[0], array2[0x3000-1]);
}
