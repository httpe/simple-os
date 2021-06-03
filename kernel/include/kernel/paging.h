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

// Variables from linker script
extern char MAP_MEM_PA_ZERO_TO[], KERNEL_VIRTUAL_END[];

void initialize_paging();

// definition various between architecture
struct page_directory_entry;
typedef struct page_directory_entry pde;

pde* curr_page_dir();
pde* alloc_page_dir();

uint32_t alloc_pages(pde* page_dir, size_t page_count, bool is_kernel, bool is_writeable);
uint32_t alloc_pages_consecutive_frames(pde* page_dir, size_t page_count, bool is_writeable, uint32_t* physical_addr);
uint32_t change_page_rw_attr(pde* page_dir, uint32_t page_index, bool is_writeable);
uint32_t alloc_pages_at(pde* page_dir, uint32_t page_index, size_t page_count, bool is_kernel, bool is_writeable);
void dealloc_pages(pde* page_dir, uint32_t page_index, size_t page_count);

uint32_t link_pages_between(pde* pd_source, uint32_t vaddr, uint32_t size, pde* pd_target, bool alloc_source_rw, bool target_rw);
void unmap_pages(pde* page_dir, uint32_t vaddr, uint32_t size);

bool is_vaddr_accessible(pde* page_dir, uint32_t vaddr, bool is_from_kernel_code, bool is_writing);
uint32_t vaddr2paddr(pde* page_dir, uint32_t vaddr);

void switch_page_directory(uint32_t physical_addr);
void set_tss(uint32_t kernel_stack_esp);

void copy_kernel_space_mapping(pde* page_dir);
pde* copy_user_space(pde* page_dir);
void free_user_space(pde* page_dir);


#endif