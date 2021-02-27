#ifndef _KERNEL_PAGING_H
#define _KERNEL_PAGING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// A page is 4KiB in size
#define PAGE_SIZE 0x1000
// Get 0-based page index from virtual address, one page is 4KiB in size
#define PAGE_INDEX_FROM_VADDR(vaddr) ((vaddr) / PAGE_SIZE)
// Get the 4KiB aligned virtual address from page index
#define VADDR_FROM_PAGE_INDEX(idx) ((idx) * PAGE_SIZE)

#define PAGE_COUNT_FROM_BYTES(n_bytes) (((n_bytes) + (PAGE_SIZE-1))/PAGE_SIZE) 

void initialize_paging();

uint32_t alloc_pages(size_t page_count, bool is_kernel, bool is_writeable);
uint32_t map_pages(uint32_t vaddr, size_t size, bool is_kernel, bool is_writeable);
void dealloc_pages(uint32_t vaddr, size_t page_count);
bool is_vaddr_accessible(uint32_t vaddr, bool is_from_kernel_code, bool is_writing);

#endif