#include <stdio.h>
#include <string.h>
#include <kernel/panic.h>
#include <kernel/common.h>

#include "isr.h"
#include "memory_bitmap.h"
#include "paging.h"



extern char KERNEL_VIRTUAL_END[];

// Flush TLB (translation lookaside buffer) for a single page
// Need flushing when a page table is changed
// source: https://wiki.osdev.org/Paging
static inline void flush_tlb(uint32_t addr) {
    asm volatile("invlpg (%0)" ::"r" (addr) : "memory");
}

// Switch page directory
// When page directory entries have been changed, can switch to oneself to flush the cache
static inline void switch_page_directory(uint32_t physical_addr) {
    asm volatile("mov %0, %%cr3": : "r"(physical_addr));
}

static void page_fault_callback(registers_t* regs) {
    UNUSED(regs);
    uint32_t cr2;
    asm volatile("mov %%cr2, %0": "=r"(cr2));
    printf("KERNEL PANIC: PAGE FAULT! Address: 0x%x", cr2);
    while (1);
}

static void install_page_fault_handler() {
    register_interrupt_handler(PAGE_FAULT_INTERRUPT, page_fault_callback);
}

// Unmapped a page, free its mapped frame
static void free_frame(uint32_t page_index) {
    uint32_t vaddr = VADDR_FROM_PAGE_INDEX(page_index);
    uint32_t page_dir_idx = page_index / PAGE_TABLE_SIZE;
    uint32_t page_table_idx = page_index % PAGE_TABLE_SIZE;
    PANIC_ASSERT(PAGE_DIR_PTR[page_dir_idx].present);
    page_t* page = &PAGE_TABLE_PTR(page_dir_idx)[page_table_idx];
    PANIC_ASSERT(page->present);
    // Fill with garbage to crash dangling reference/pointer earlier
    memset((char*)vaddr, 1, PAGE_SIZE);
    clear_frame(page->frame);
    page->present = 0;
    // flush
    flush_tlb(vaddr);
    printf("Page frame freed: PD[%d]:PT[%d]:Frame[0x%x]\n", page_dir_idx, page_table_idx, page->frame);
    
}

// Allocate physical space for a page (if not already mapped)
// return: allocated physical frame index
static uint32_t alloc_frame(uint32_t page_index, bool is_kernel, bool is_writeable) {
    uint32_t page_dir_idx = page_index / PAGE_TABLE_SIZE;
    uint32_t page_table_idx = page_index % PAGE_TABLE_SIZE;

    if (!PAGE_DIR_PTR[page_dir_idx].present) {
        // Page table not allocated, create it first
        uint32_t idx = first_free_frame(); // idx is now the index of the first free frame.
        set_frame(idx);                    // this frame is now ours!
        page_directory_entry_t page_dir_entry = { .present = 1, .rw = 1, .user = 1, .page_table_addr = idx };
        PAGE_DIR_PTR[page_dir_idx] = page_dir_entry;
        switch_page_directory(PAGE_DIR_PHYSICAL_ADDR);// flush
        memset(PAGE_TABLE_PTR(page_dir_idx), 0, sizeof(page_t) * PAGE_TABLE_SIZE);
    }
    if (!PAGE_TABLE_PTR(page_dir_idx)[page_table_idx].present) { // This should always be true
        // Not present means it hasn't been mapped to physical space
        uint32_t idx = first_free_frame(); // idx is now the index of the first free frame.
        set_frame(idx);                    // this frame is now ours!
        page_t page = { .present = 1, .user = !is_kernel, .rw = is_writeable, .frame = idx };
        PAGE_TABLE_PTR(page_dir_idx)[page_table_idx] = page;
        flush_tlb(VADDR_FROM_PAGE_INDEX(page_index));
        printf("Page frame allocated: PD[%d]:PT[%d]:Frame[0x%x]\n", page_dir_idx, page_table_idx, idx);
    }
    return PAGE_TABLE_PTR(page_dir_idx)[page_table_idx].frame;
}

// Find contiguous pages that have not been mapped
// If is for kernel, search vaddr space after KERNEL_VIRTUAL_END
// return: the first page index of the contiguous unmapped virtual memory space
static uint32_t first_contiguous_page_index(size_t page_count, bool is_kernel) {
    uint32_t page_dir_idx_0;
    if(is_kernel) {
        page_dir_idx_0 = PAGE_INDEX_FROM_VADDR((uint32_t) KERNEL_VIRTUAL_END) / PAGE_TABLE_SIZE;
    } else {
        page_dir_idx_0 = 0;
    }
    uint32_t contiguous_page_count = 0;
    for (uint32_t page_dir_idx = page_dir_idx_0; page_dir_idx < PAGE_DIR_SIZE; page_dir_idx++) {
        if (PAGE_DIR_PTR[page_dir_idx].present == 0) {
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
            if (PAGE_TABLE_PTR(page_dir_idx)[page_table_idx].present == 0) {
                if (contiguous_page_count + 1 >= page_count) {
                    return page_dir_idx * PAGE_TABLE_SIZE + page_table_idx - contiguous_page_count;
                }
                contiguous_page_count++;
            } else {
                contiguous_page_count = 0;
            }
        }
    }

    printf("KERNEL PANIC: Find contiguous VA failed\n");
    while (1);

}

void dealloc_pages(uint32_t vaddr, size_t page_count) {
    if (page_count == 0) {
        return;
    }

    uint32_t page_index = PAGE_INDEX_FROM_VADDR(vaddr);
    for (uint32_t idx = page_index; idx < page_index + page_count; idx++) {
        free_frame(idx);
    }
}

uint32_t alloc_pages(size_t page_count, bool is_kernel, bool is_writeable) {
    if (page_count == 0) {
        return 0;
    }

    uint32_t page_index = first_contiguous_page_index(page_count, is_kernel);
    // printf("kmalloc:page_index=%d\n", page_index);

    for (uint32_t idx = page_index; idx < page_index + page_count; idx++) {
        alloc_frame(idx, is_kernel, is_writeable);
    }

    return VADDR_FROM_PAGE_INDEX(page_index);
}

bool is_vaddr_accessible(uint32_t vaddr, bool is_from_kernel_code, bool is_writing) {
    uint32_t page_index = PAGE_INDEX_FROM_VADDR(vaddr);
    uint32_t page_dir_idx = page_index / PAGE_TABLE_SIZE;
    uint32_t page_table_idx = page_index % PAGE_TABLE_SIZE;
    if (!PAGE_DIR_PTR[page_dir_idx].present) {
        return false;
    }
    if (!PAGE_TABLE_PTR(page_dir_idx)[page_table_idx].present) {
        return false;
    }
    // Kernel can do anything, assuming we are not in write protected mode
    // "The WP bit in CR0 determines if this is only applied to userland, 
    //   always giving the kernel write access (the default) or both userland and the kernel 
    //   (see Intel Manuals 3A 2-20)"
    if (is_from_kernel_code) {
        return true;
    }
    if (PAGE_DIR_PTR[page_dir_idx].rw < is_writing) {
        return false;
    }
    if (!PAGE_DIR_PTR[page_dir_idx].user < is_from_kernel_code) {
        return false;
    }
    return true;
}

void initialize_paging() {
    install_page_fault_handler();

    printf("Boot page dir physical addr: 0x%x\n", PAGE_DIR_PHYSICAL_ADDR);
    page_directory_entry_t page_dir_entry_0 = PAGE_DIR_PTR[0xC0000000 >> 22];
    printf("Boot page table physical addr: 0x%x\n", page_dir_entry_0.page_table_addr << 12);
    page_t page_table_entry_0 = PAGE_TABLE_PTR(0xC0000000 >> 22)[0];
    printf("Boot page table entry 0 point to physical addr: 0x%x\n", page_table_entry_0.frame << 12);

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
