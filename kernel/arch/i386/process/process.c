#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <arch/i386/kernel/process.h>
#include <kernel/paging.h>
#include <kernel/panic.h>
#include <arch/i386/kernel/segmentation.h>
#include <kernel/memory_bitmap.h>

// defined in interrupt.asm
extern void int_ret(void);
// exposed by linker script for start_init.asm
extern char START_INIT_PHY_BEGIN[], START_INIT_VIRTUAL_BEGIN[], START_INIT_RELOC_BEGIN[], START_INIT_SIZE[];
// defined in switch_kernel_context.asm
extern void switch_kernel_context(struct context **old, struct context *new);

struct {
  proc proc[N_PROCESS];
} process_table;

static uint32_t next_pid = 1;
proc* current_process = NULL;
context* kernel_boot_context = NULL;

void init_process() 
{
    //TODO: Add process specific initialization
    // This function is called after stack changed to process's kernel stack
    // and the page dir changed to process's own dir
}

// Ref: xv6/proc.c
// Allocate a new process
proc* create_process()
{
    proc* p = NULL;
    for(int i=0;i<N_PROCESS;i++) {
        if(process_table.proc[i].state == UNUSED) {
            p = &process_table.proc[i];
            break;
        }
        if(i==N_PROCESS-1) {
            return NULL;
        }
    }

    p->pid = next_pid++;
    p->state = EMBRYO;
    // allocate process's kernel stack
    p->kernel_stack = (char*) alloc_pages(curr_page_dir(), N_KERNEL_STACK_PAGE_SIZE, true, true);
    uint32_t stack_size = PAGE_SIZE*N_KERNEL_STACK_PAGE_SIZE;
    memset(p->kernel_stack, 0, stack_size);
    char* sp = p->kernel_stack + stack_size;
    // setup trap frame for switching back to user space
    sp -= sizeof(*p->tf);
    p->tf = (trapframe*) sp;
    // setup eip for returning from init_process to be int_ret
    sp -= sizeof(sp);
    (*(uint32_t*)sp) = (uint32_t) int_ret;
    // setup context for kernel space switching to init_process
    sp -= sizeof(*p->context);
    p->context = (context*) sp;
    p->context->eip = (uint32_t) init_process;
    // p->eip shall be setup later
    // then the call chain will be scheduler -> init_process -> int_ret -> (p->eip)
    
    return p;
}

// Create and initialize the first (user space) process to run
void init_first_process()
{
    proc* p = create_process();
    // allocate page dir
    p->page_dir = (page_directory_entry_t*) alloc_pages(curr_page_dir(), 1, true, true);
    memset(p->page_dir, 0, sizeof(page_directory_entry_t)*PAGE_DIR_SIZE);

    // allocate/map user space page for the start_init routine
    PANIC_ASSERT((uint32_t)START_INIT_RELOC_BEGIN % 0x1000 == 0);
    PANIC_ASSERT((uint32_t)START_INIT_SIZE < PAGE_SIZE/2); // make sure we can use ending part of the page as stack
    PANIC_ASSERT((uint32_t)START_INIT_SIZE > 0);
    PANIC_ASSERT(0!=*(char*)START_INIT_VIRTUAL_BEGIN);
    // the START_INIT_VIRTUAL_BEGIN is not aligned with page boundary, so we copied it to the a new frame
    uint32_t vaddr = alloc_pages(curr_page_dir(), 1, true, true);
    memset((char*) vaddr, 0, PAGE_SIZE);
    memmove((char*) vaddr, START_INIT_VIRTUAL_BEGIN, (uint32_t)START_INIT_SIZE);
    // map the frame int process's page dir to START_INIT_RELOC_BEGIN
    uint32_t paddr = vaddr2paddr(curr_page_dir(), vaddr);
    uint32_t page_idx = PAGE_INDEX_FROM_VADDR((uint32_t) START_INIT_RELOC_BEGIN);
    uint32_t entry = page_idx*PAGE_SIZE;
    map_frame_to_dir(p->page_dir, page_idx, FRAME_INDEX_FROM_ADDR(paddr), false, true);
    unmap_page_from_dir(curr_page_dir(), PAGE_INDEX_FROM_VADDR(vaddr));
    PANIC_ASSERT(vaddr2paddr(p->page_dir, (uint32_t) START_INIT_RELOC_BEGIN) == paddr);

    // Setup trap frame for iret into user space
    p->tf->cs = SEG_SELECTOR(SEG_UCODE, DPL_USER);
    p->tf->ds = SEG_SELECTOR(SEG_UDATA, DPL_USER);
    p->tf->es = p->tf->ds;
    p->tf->ss = p->tf->ds;
    p->tf->eflags = FL_IF;
    p->tf->esp = entry + PAGE_SIZE; // use the ending part of the page as stack 
    p->tf->eip = (uint32_t) entry;

    p->state = RUNNABLE;
}

static void switch_process_memory_mapping(proc* p)
{
    set_tss((uint32_t) p->kernel_stack + PAGE_SIZE*N_KERNEL_STACK_PAGE_SIZE);
    // kernel space shall have identical mapping
    copy_kernel_space_mapping(p->page_dir);
    uint32_t page_dir_paddr = vaddr2paddr(curr_page_dir(), (uint32_t) p->page_dir);
    switch_page_directory(page_dir_paddr);
}

void scheduler()
{
    proc* p;
    while(1) {
        for(p = process_table.proc; p < &process_table.proc[N_PROCESS]; p++){
            if(p->state != RUNNABLE)
                continue;
            current_process = p;
            switch_process_memory_mapping(p);
            p->state = RUNNING;
            switch_kernel_context(&kernel_boot_context, p->context);
            // TODO
            PANIC("Returned to scheduler");
        }
    }

    
}