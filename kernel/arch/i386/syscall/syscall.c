#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <kernel/heap.h>
#include <kernel/panic.h>
#include <kernel/multiboot.h>
#include <kernel/ata.h>
#include <kernel/elf.h>
#include <kernel/tar.h>
#include <common.h>

#include <arch/i386/kernel/syscall.h>
#include <arch/i386/kernel/process.h>


extern char MAP_MEM_PA_ZERO_TO[];

// max number of command line arguments
#define MAX_ARGC 10
// user program stack size in pages
#define USER_STACK_PAGE_SIZE 1

// Copied from bootloader
// The whole bootloader binary is assumed to take the first 16 sectors
// must be in sync of BOOTLOADER_MAX_SIZE in Makefile (of bootloader)
#define BOOTLOADER_SECTORS 16
#define SECTOR_SIZE 512

// Load a file from the tar file system
//
// LBA: 0-based Linear Block Address, 28bit LBA shall be between 0 to 0x0FFFFFFF
// filename: get a file with this name from the tar file system
// buffer: load the file found into this address
//
// return: file size of the loaded file
int tar_loopup_lazy(uint32_t LBA, char* filename, char** file_buffer) {
    unsigned char* sector_buffer = kmalloc(SECTOR_SIZE);
    uint32_t max_lba = get_total_28bit_sectors();

    while(1) {
        if (LBA >= max_lba) {
            kfree(sector_buffer);
            return TAR_ERR_LBA_GT_MAX_SECTOR;
        }
        read_sectors_ATA_28bit_PIO((uint16_t*)sector_buffer, LBA, 1);
        int match = tar_match_filename(sector_buffer, filename);
        if (match == TAR_ERR_NOT_USTAR) {
            kfree(sector_buffer);
            return TAR_ERR_NOT_USTAR;
        } else {
            int filesize = tar_get_filesize(sector_buffer);
            int size_in_sector = ((filesize + (SECTOR_SIZE-1)) / SECTOR_SIZE) + 1; // plus one for the meta sector
            if (match == TAR_ERR_FILE_NAME_NOT_MATCH) {
                LBA += size_in_sector;
                continue;
            } else {
                *file_buffer = kmalloc(SECTOR_SIZE*size_in_sector);
                read_sectors_ATA_28bit_PIO((uint16_t*)*file_buffer, LBA + 1, size_in_sector);
                kfree(sector_buffer);
                return filesize;
            }
        }
    }
}

int sys_exec(trapframe* r)
{
    // TODO: parameter security check
    uint32_t num = *(uint32_t*) r->esp;
    char* path = (char*) *(uint32_t*) (r->esp + 4);
    char** argv = (char**) *(uint32_t*) (r->esp + 8);
    printf("SYS_EXEC: eip: %u, path: %s\n", num, path);

    // Load executable from tar file system
    // TODO: replace by real file system
    char* file_buffer = NULL;
    int program_size = tar_loopup_lazy(BOOTLOADER_SECTORS, path, &file_buffer);
    if(program_size <= 0) {
        printf("SYS_EXEC: File not found\n");
        kfree(file_buffer);
        return -1;
    }
	printf("SYS_EXEC: program size: %u\n", program_size);
    if (!is_elf(file_buffer)) {
        printf("SYS_EXEC: Invalid program\n");
        kfree(file_buffer);
        return -1;
    }

    // allocate page dir
    pde* page_dir = (pde*) alloc_pages(curr_page_dir(), 1, true, true);
    memset(page_dir, 0, sizeof(pde)*PAGE_DIR_SIZE);

    // parse and load ELF binary
    uint32_t vaddr_ub = 0;
    uint32_t entry_point = load_elf(page_dir, file_buffer, &vaddr_ub);

    // allocate stack to just below the higher half kernel mapping
    uint32_t ustack_end = (uint32_t) MAP_MEM_PA_ZERO_TO;
    uint32_t ustack_size = PAGE_SIZE*USER_STACK_PAGE_SIZE;
    alloc_pages_at(page_dir, ustack_end - ustack_size, ustack_size, false, true);
    
    // user stack, mimic a normal function call
    uint32_t ustack[3+MAX_ARGC+1]; // +1: add a termination zero
    ustack[0] = 0x0; // fake return PC
    ustack[2] = vaddr_ub;
    int argc;
    for(argc=0; argc<MAX_ARGC; argc++) {
        if(argv[argc] == 0) {
            ustack[3 + argc] = 0;
            break;
        }
        // copy the argv content to memory right above any mapped memory specified by the ELF binary
        uint32_t size = strlen(argv[argc]) + 1;
        char* dst = (char*) link_pages(page_dir, vaddr_ub, size, curr_page_dir(), false);
        memmove(dst, argv, size);
        unmap_pages(curr_page_dir(), (uint32_t) dst, size);
        // memmove_into_dir(page_dir, vaddr_ub, argv, size, false);
        ustack[3 + argc] = vaddr_ub;
        vaddr_ub += size;
    }
    ustack[1] = argc;
    uint32_t stack_content_size = sizeof(void*)*(3 + argc + 1);
    uint32_t ustack_esp = ustack_end - stack_content_size;
    char* dst = (char*) link_pages(page_dir, ustack_esp, stack_content_size, curr_page_dir(), true);
    memmove(dst, ustack, stack_content_size);
    unmap_pages(curr_page_dir(), (uint32_t) dst, stack_content_size);

    // maintain trapframe
    proc* p = curr_proc();
    p->tf->esp = ustack_esp;
    p->tf->eip = entry_point;

    // switch to new page dir
    pde* old_page_dir = p->page_dir;
    p->page_dir = page_dir;
    switch_process_memory_mapping(p);
    // free_user_space(old_page_dir); // free frames occupied by the old page dir

    PANIC_ASSERT(p->page_dir != old_page_dir);
    PANIC_ASSERT((uint32_t) PAGE_DIR_PHYSICAL_ADDR != vaddr2paddr(curr_page_dir(), (uint32_t) old_page_dir));
    PANIC_ASSERT(vaddr2paddr(curr_page_dir(), (uint32_t) curr_page_dir()) == (uint32_t) PAGE_DIR_PHYSICAL_ADDR);
    PANIC_ASSERT((uint32_t) PAGE_DIR_PHYSICAL_ADDR == vaddr2paddr(curr_page_dir(), (uint32_t) page_dir));
    PANIC_ASSERT(is_vaddr_accessible(curr_page_dir(), p->tf->eip, false, false));
    PANIC_ASSERT(is_vaddr_accessible(curr_page_dir(), p->tf->esp, false, false));
    
    return 0;
}

int sys_print(trapframe* r)
{
    char* str = (char*) *(uint32_t*) (r->esp + 4);
    printf("SYS_PRINT: %s\n", str);
    return 0;
}

int sys_yield(trapframe* r)
{
    UNUSED_ARG(r);
    yield();
    return 0;
}

int sys_fork(trapframe* r)
{
    UNUSED_ARG(r);
    proc* p_new = create_process();
    proc* p_curr = curr_proc();
    p_new->page_dir = copy_user_space(p_curr->page_dir);
    p_new->parent = p_curr;
    *p_new->tf = *p_curr->tf;
    // child process will have return value zero from fork
    p_new->tf->eax = 0;
    p_new->state = PROC_STATE_RUNNABLE;
    // return to parent process with child's pid
    return p_new->pid;
}

int sys_exit(trapframe* r)
{
    int32_t exit_code = *(int32_t*) (r->esp + 4);
    printf("PID %u exiting with code %d\n", curr_proc()->pid, exit_code);
    exit(exit_code);
    PANIC("Returned to exited process\n");
}

int sys_wait(trapframe* r)
{
    UNUSED_ARG(r);
    return wait();
}

void syscall_handler(trapframe* r)
{
    // trapframe r will be pop when returning to user space
    // so r->eax will be the return value of the syscall  
    switch (r->eax)
    {
    case SYS_EXEC:
        r->eax = sys_exec(r);
        break;
    case SYS_PRINT:
        r->eax = sys_print(r);
        break;
    case SYS_YIELD:
        r->eax = sys_yield(r);
        break;
    case SYS_FORK:
        r->eax = sys_fork(r);
        break;
    case SYS_EXIT:
        r->eax = sys_exit(r);
        break;
    case SYS_WAIT:
        r->eax = sys_wait(r);
        break;
    default:
        printf("Unrecognized Syscall: %d\n", r->eax);
        PANIC("Unrecognized Syscall");
        r->eax = -1;
        break;
    }
}

