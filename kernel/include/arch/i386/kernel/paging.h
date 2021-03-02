#ifndef _ARCH_I386_KERNEL_PAGING_H
#define _ARCH_I386_KERNEL_PAGING_H

#include <stdint.h>

////////////////////////////////
// Paging
////////////////////////////////


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
   uint32_t avaible         : 3;   // Available to OS
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



#endif