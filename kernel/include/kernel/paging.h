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

// definition various between architecture
struct page_directory_entry;

struct page_directory_entry* curr_page_dir();
uint32_t find_contiguous_free_pages(struct page_directory_entry* page_dir, size_t page_count, bool is_kernel);
uint32_t map_frame_to_dir(struct page_directory_entry* page_dir, uint32_t page_index, uint32_t frame_index, bool is_kernel, bool is_writeable);
uint32_t unmap_page_from_dir(struct page_directory_entry* page_dir, uint32_t page_index);
uint32_t alloc_pages(struct page_directory_entry* page_dir, size_t page_count, bool is_kernel, bool is_writeable);
uint32_t alloc_pages_at(struct page_directory_entry* page_dir, uint32_t vaddr, size_t size, bool is_kernel, bool is_writeable);
void dealloc_pages(struct page_directory_entry* page_dir, uint32_t vaddr, size_t page_count);
bool is_vaddr_accessible(struct page_directory_entry* page_dir, uint32_t vaddr, bool is_from_kernel_code, bool is_writing);
int64_t vaddr2paddr(struct page_directory_entry* page_dir, uint32_t vaddr);
extern struct page_directory_entry* kernel_page_dir;

#endif