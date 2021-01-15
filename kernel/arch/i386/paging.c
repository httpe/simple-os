#include "paging.h"
#include "isr.h"
#include <stdio.h>

#define PAGE_TO_MAP 4
#define PAGE_FAULT_INTERRUPT 14 

page_table_t page_tables[PAGE_TO_MAP] __attribute__((aligned(4096)));
page_directory_t first_page_directory __attribute__((aligned(4096)));

static void page_fault_callback(registers_t *regs) {
    uint32_t cr2;
    asm volatile("mov %%cr2, %0": "=r"(cr2));
    printf("KERNEL PANIC: PAGE FAULT! Address: 0x%x", cr2);
    while(1);
}

void initialize_paging() {
    //set each entry to not present
    unsigned int i;
    for(i = 0; i < 1024; i++)
    {
        // This sets the following flags to the pages:
        //   Supervisor: Only kernel-mode can access them
        //   Write Enabled: It can be both read from and written to
        //   Not Present: The page table is not present
        first_page_directory.tablesPhysical[i] = (page_directory_entry_t) {.present = 0, .rw = 1, .user = 0, .page_table_addr = 0};
    }

    // holds the physical address where we want to start mapping these pages to.
    // in this case, we want to map these pages to the very beginning of memory.

    //we will fill all 1024 entries in the table, mapping 4 megabytes
    for(unsigned int j = 0; j < PAGE_TO_MAP; j++) {
        for(i = 0; i < 1024; i++)
        {
            // As the address is page aligned, it will always leave 12 bits zeroed.
            // Those bits are used by the attributes ;)
            // attributes: supervisor level, read/write, present.
            // Frame: Identity Paging
            page_tables[j].pages[i] = (page_t) {.present = 1, .user = 0, .rw = 1, .frame = j*1024 + i};
        }
        first_page_directory.tablesPhysical[j] = (page_directory_entry_t) {.present = 1, .rw = 1, .user = 0, .page_table_addr = ((uint32_t) &page_tables[j]) >> 12};
    }

    first_page_directory.physicalAddr = (uint32_t) first_page_directory.tablesPhysical;

    asm volatile("mov %0, %%cr3":: "r"(&first_page_directory.tablesPhysical));
    uint32_t cr0;
    asm volatile("mov %%cr0, %0": "=r"(cr0));
    cr0 |= 0x80000000; // Enable paging!
    asm volatile("mov %0, %%cr0":: "r"(cr0));

}

void install_page_fault_handler() {
    register_interrupt_handler(PAGE_FAULT_INTERRUPT, page_fault_callback);
}