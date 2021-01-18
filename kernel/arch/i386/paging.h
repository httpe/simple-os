#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Ref: https://blog.inlow.online/2019/01/21/Paging/
// Ref: http://www.jamesmolloy.co.uk/tutorial_html/6.-Paging.html

// Entries per page directory
#define PAGE_DIR_SIZE 1024
// Entries per page table
#define PAGE_TABLE_SIZE 1024

// A page is 4KiB in size
#define PAGE_SIZE 0x1000
// Get 0-based page index from virtual address, one page is 4KiB in size
#define PAGE_INDEX_FROM_VADDR(vaddr) ((vaddr) / PAGE_SIZE)
// Get the 4KiB aligned virtual address from page index
#define VADDR_FROM_PAGE_INDEX(idx) ((idx) * PAGE_SIZE)

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
   uint32_t avaible         : 3;   // Available to OS
   uint32_t page_table_addr : 20;  // Physical address to the page table (shifted right 12 bits)
} page_directory_entry_t;


// By using the recursive page directory trick, we can access page dir entry via
// virtual address [10 bits of 1;10bits of 1;0, 4, 8 etc. 12bits of page dir index * 4]
// where [10 bits of 1;10bits of 1;12bits of 0] = 0xFFFFF000
#define PAGE_DIR_PTR ((page_directory_entry_t*) 0xFFFFF000)
// To access page table entries, do similarly [10 bits of 1; 10bits of page dir index for the table; 0, 4, 8 etc. 12bits of page table index * 4]
#define PAGE_TABLE_PTR(idx) ((page_t*) (0xFFC00000 + ((idx) << 12)))
// We can also get page directory's physical address by acessing it's last entry, which point to itself, thus being recursive
#define PAGE_DIR_PHYSICAL_ADDR (((page_directory_entry_t*) 0xFFFFF000)[1023].page_table_addr << 12)

#define PAGE_COUNT_FROM_BYTES(n_bytes) ((n_bytes)/PAGE_SIZE + ((n_bytes) % PAGE_SIZE) != 0) 

// typedef struct page_table
// {
//    page_t pages[1024];
// } page_table_t;

// typedef struct page_directory
// {
//    page_directory_entry_t page_tables[1024];
// } page_directory_t;

// typedef struct page_directory
// {
//    /**
//       Array of pointers to pagetables.
//    **/
//    page_table_t *tables[1024];
//    /**
//       Array of pointers to the pagetables above, but gives their *physical*
//       location, for loading into the CR3 register.
//    **/
//    page_directory_entry_t tablesPhysical[1024];
//    /**
//       The physical address of tablesPhysical. This comes into play
//       when we get our kernel heap allocated and the directory
//       may be in a different location in virtual memory.
//    **/
//    uint32_t physicalAddr;
// } page_directory_t;


void initialize_paging();
void install_page_fault_handler();

uint32_t alloc_pages(size_t page_count, bool is_kernel, bool is_writeable);
uint32_t alloc_frame(uint32_t page_index, bool is_kernel, bool is_writeable);
void free_frame(uint32_t page_index);

bool is_vaddr_accessible(uint32_t vaddr, bool is_from_kernel_code, bool is_writing);

#endif