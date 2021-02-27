#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


////////////////////////////////
// Segmentation (GDT/TSS)
////////////////////////////////

// From: xv6/mmu.h

// various segment selectors.
#define SEG_NULL 0   // null segment
#define SEG_KCODE 1  // kernel code
#define SEG_KDATA 2  // kernel data+stack
#define SEG_UCODE 3  // user code
#define SEG_UDATA 4  // user data+stack
#define SEG_TSS   5  // this process's task state

// segment selector for CS/DS/ES etc.
// See https://wiki.osdev.org/Selector (RPL=Requested Privilege Level)
#define SEG_SELECTOR(seg,rpl) (((seg) << 3) + rpl)

// global var gdt[NSEGS] holds the above segments.
#define NSEGS     6

// Segment Descriptor
typedef struct segdesc {
  uint32_t lim_15_0 : 16;  // Low bits of segment limit
  uint32_t base_15_0 : 16; // Low bits of segment base address
  uint32_t base_23_16 : 8; // Middle bits of segment base address
  uint32_t type : 4;       // Segment type (see STS_ constants)
  uint32_t s : 1;          // 0 = system, 1 = application
  uint32_t dpl : 2;        // Descriptor Privilege Level
  uint32_t p : 1;          // Present
  uint32_t lim_19_16 : 4;  // High bits of segment limit
  uint32_t avl : 1;        // Unused (available for software use)
  uint32_t rsv1 : 1;       // Reserved
  uint32_t db : 1;         // 0 = 16-bit segment, 1 = 32-bit segment
  uint32_t g : 1;          // Granularity: limit scaled by 4K when set
  uint32_t base_31_24 : 8; // High bits of segment base address
} segdesc;

// For normal segments (4K granularity, so lim >> 12, lim >> 28)
#define SEG(type, base, lim, dpl) (struct segdesc)    \
{ ((lim) >> 12) & 0xffff, (uint32_t)(base) & 0xffff,      \
  ((uint32_t)(base) >> 16) & 0xff, type, 1, dpl, 1,       \
  (uint32_t)(lim) >> 28, 0, 0, 1, 1, (uint32_t)(base) >> 24 }
// For TSS segment (1B granularity)
#define TSS_SEG(type, base, lim, dpl) (struct segdesc)  \
{ (uint32_t)(lim) & 0xffff, (uint32_t)(base) & 0xffff,              \
  ((uint32_t)(base) >> 16) & 0xff, type, 0, dpl, 1,       \
  (uint32_t)(lim) >> 16, 0, 0, 1, 0, (uint32_t)(base) >> 24 }

#define DPL_KERNEL  0x0     // Kernel DPL
#define DPL_USER    0x3     // User DPL

// Application segment type bits
#define STA_X       0x8     // Executable segment
#define STA_W       0x2     // Writeable (non-executable segments)
#define STA_R       0x2     // Readable (executable segments)

// System segment type bits
#define STS_T32A    0x9     // Available 32-bit TSS
#define STS_IG32    0xE     // 32-bit Interrupt Gate
#define STS_TG32    0xF     // 32-bit Trap Gate

// Task state segment format
typedef struct task_state {
  uint16_t link;         // Old ts selector
  uint16_t padding0;
  uint32_t esp0;        // Stack pointers after an increase in privilege level 
  uint16_t ss0;         // Segment selectors after an increase in privilege level
  uint16_t padding1;
  uint32_t *esp1;
  uint16_t ss1;
  uint16_t padding2;
  uint32_t *esp2;
  uint16_t ss2;
  uint16_t padding3;
  uint32_t cr3;         // Page directory base
  uint32_t *eip;        // Saved state from last task switch
  uint32_t eflags;
  uint32_t eax;         // More saved state (registers)
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t *esp;
  uint32_t *ebp;
  uint32_t esi;
  uint32_t edi;
  uint16_t es;          // Even more saved state (segment selectors)
  uint16_t padding4;
  uint16_t cs;
  uint16_t padding5;
  uint16_t ss;
  uint16_t padding6;
  uint16_t ds;
  uint16_t padding7;
  uint16_t fs;
  uint16_t padding8;
  uint16_t gs;
  uint16_t padding9;
  uint16_t ldtr;
  uint16_t padding10;
  uint16_t t;           // Trap on task switch
  uint16_t iomb;        // I/O map base address
} task_state;


////////////////////////////////
// Paging
////////////////////////////////


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
   uint32_t page_table_frame : 20; // Physical address to the page table (shifted right 12 bits)
} page_directory_entry_t;


// By using the recursive page directory trick, we can access page dir entry via
// virtual address [10 bits of 1;10bits of 1;0, 4, 8 etc. 12bits of page dir index * 4]
// where [10 bits of 1;10bits of 1;12bits of 0] = 0xFFFFF000
#define PAGE_DIR_PTR ((page_directory_entry_t*) 0xFFFFF000)
// To access page table entries, do similarly [10 bits of 1; 10bits of page dir index for the table; 0, 4, 8 etc. 12bits of page table index * 4]
#define PAGE_TABLE_PTR(idx) ((page_t*) (0xFFC00000 + ((idx) << 12)))
// We can also get page directory's physical address by acessing it's last entry, which point to itself, thus being recursive
#define PAGE_DIR_PHYSICAL_ADDR (((page_directory_entry_t*) 0xFFFFF000)[1023].page_table_frame << 12)

#define PAGE_COUNT_FROM_BYTES(n_bytes) (((n_bytes) + (PAGE_SIZE-1))/PAGE_SIZE) 

void initialize_paging();

uint32_t alloc_pages(size_t page_count, bool is_kernel, bool is_writeable);
uint32_t map_pages(uint32_t vaddr, size_t size, bool is_kernel, bool is_writeable);
void dealloc_pages(uint32_t vaddr, size_t page_count);
bool is_vaddr_accessible(uint32_t vaddr, bool is_from_kernel_code, bool is_writing);
uint32_t vaddr2paddr(page_directory_entry_t* page_dir, uint32_t vaddr);

#endif