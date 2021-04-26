#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <common.h>
#include <kernel/heap.h>
#include <kernel/panic.h>
#include <kernel/multiboot.h>
#include <kernel/ata.h>
#include <kernel/elf.h>
#include <kernel/tar.h>
#include <kernel/vfs.h>
#include <kernel/file_system.h>
#include <kernel/process.h>
#include <kernel/paging.h>
#include <kernel/stat.h>
#include <kernel/time.h>
#include <arch/i386/kernel/isr.h>
#include <arch/i386/kernel/cpu.h>

extern char MAP_MEM_PA_ZERO_TO[];

// max number of command line arguments
#define MAX_ARGC 10
// user program stack size in pages
#define USER_STACK_PAGE_SIZE 1

int sys_exec(trapframe* r)
{
    // TODO: parameter security check
    uint32_t num = *(uint32_t*) r->esp;
    char* path = (char*) *(uint32_t*) (r->esp + 4);
    char** argv = (char**) *(uint32_t*) (r->esp + 8);
    printf("SYS_EXEC: eip: %u, path: %s\n", num, path);

    // Load executable from file system

    fs_stat st = {0};
    int fs_res = fs_getattr_path(path, &st);
    if(fs_res < 0) {
        printf("SYS_EXEC: Cannot open the file\n");
        return -1;
    }
    if(st.size == 0) {
        printf("SYS_EXEC: File has size zero\n");
        return -1;
    }
    printf("SYS_EXEC: program size: %u\n", st.size);

    int fd = fs_open(path, 0);
    if(fd < 0) {
        return -1;
    }
    char* file_buffer = kmalloc(st.size);
    fs_res = fs_read(fd, file_buffer, st.size);
    if(fs_res < 0) {
        return -1;
    }
    fs_close(fd);
    

    if (!is_elf(file_buffer)) {
        printf("SYS_EXEC: Invalid program\n");
        kfree(file_buffer);
        return -1;
    }

    // allocate page dir
    pde* page_dir = alloc_page_dir();

    // parse and load ELF binary
    uint32_t vaddr_ub = 0;
    uint32_t entry_point = load_elf(page_dir, file_buffer, &vaddr_ub);

    // allocate stack to just below the higher half kernel mapping
    uint32_t esp = (uint32_t) MAP_MEM_PA_ZERO_TO;
    uint32_t ustack_start = alloc_pages_at(page_dir, PAGE_INDEX_FROM_VADDR(esp) - USER_STACK_PAGE_SIZE, USER_STACK_PAGE_SIZE, false, true);
    // allocate one read-only page below the user stack to catch stack overflow or heap over growth
    alloc_pages_at(page_dir, PAGE_INDEX_FROM_VADDR(ustack_start) - 1, 1, false, false);

    // copy argv strings to the high end of the stack area
    char* ustack_dst = (char*) link_pages(page_dir, ustack_start, PAGE_SIZE*USER_STACK_PAGE_SIZE, curr_page_dir(), false);

    uint32_t ustack[3+MAX_ARGC+1]; // +1: add a termination zero
    int argc;
    for(argc=0; argc<MAX_ARGC; argc++) {
        if(argv[argc] == 0) {
            ustack[3 + argc] = 0;
            break;
        }
        // copy the argv content to memory right above any mapped memory specified by the ELF binary
        uint32_t size = strlen(argv[argc]) + 1;
        // & ~3 to maintain 4 bytes alignment
        esp = (esp - size) & ~3;
        if(esp < ustack_start + sizeof(void*)*(3 + argc + 1 + 1)) {
            // stack overflow, no room to store 3 (fake return PC, argc, argv) + argc+1 (pointer to previous arguments plus pointer to this arg)  + 1 (a terminating zero)
            unmap_pages(curr_page_dir(), ustack_start, PAGE_SIZE*USER_STACK_PAGE_SIZE);
            free_user_space(page_dir);
            return -1;
        }
        memmove(ustack_dst + (esp - ustack_start), argv[argc], size);
        ustack[3 + argc] = esp;
    }
    
    // user stack, mimic a normal function call
    ustack[0] = 0xFFFFFFFF; // fake return PC
    ustack[1] = argc;
    ustack[2] = esp - sizeof(void*)*(argc + 1);

    uint32_t rem_stack_size = sizeof(void*)*(3 + argc + 1);
    esp -= rem_stack_size;
    memmove(ustack_dst + (esp - ustack_start), ustack, rem_stack_size);

    // maintain trapframe
    proc* p = curr_proc();
    p->tf->esp = esp;
    p->tf->eip = entry_point;

    p->size = (vaddr_ub + (PAGE_SIZE - 1))/PAGE_SIZE * PAGE_SIZE;
    p->orig_size = p->size;

    // switch to new page dir
    pde* old_page_dir = p->page_dir;
    p->page_dir = page_dir;
    switch_process_memory_mapping(p);
    free_user_space(old_page_dir); // free frames occupied by the old page dir

    PANIC_ASSERT(p->page_dir != old_page_dir);
    PANIC_ASSERT((uint32_t) vaddr2paddr(curr_page_dir(), (uint32_t) curr_page_dir()) != vaddr2paddr(curr_page_dir(), (uint32_t) old_page_dir));
    PANIC_ASSERT((uint32_t) vaddr2paddr(curr_page_dir(), (uint32_t) curr_page_dir()) == vaddr2paddr(curr_page_dir(), (uint32_t) page_dir));
    PANIC_ASSERT(is_vaddr_accessible(curr_page_dir(), p->tf->eip, false, false));
    PANIC_ASSERT(is_vaddr_accessible(curr_page_dir(), p->tf->esp, false, false));
    
    return 0;
}

// increase/decrease process image size
int sys_sbrk(trapframe* r)
{
    int32_t delta = *(int32_t*) (r->esp + 4);
    
    proc* p = curr_proc();
    uint32_t orig_size = p->size;
    uint32_t new_size = p->size + delta;
    if(new_size < p->orig_size || new_size < p->orig_size) {
        return -1;
    } 
    p->size = new_size;
    uint32_t orig_last_pg_idx = PAGE_INDEX_FROM_VADDR(orig_size - 1);
    uint32_t new_last_pg_idx =  PAGE_INDEX_FROM_VADDR(new_size - 1);
    if(new_last_pg_idx > orig_last_pg_idx) {
        alloc_pages_at(p->page_dir, orig_last_pg_idx + 1, new_last_pg_idx - orig_last_pg_idx, false, true);
    } else if(new_last_pg_idx < orig_last_pg_idx) {
        dealloc_pages(p->page_dir, new_last_pg_idx + 1, orig_last_pg_idx - new_last_pg_idx); 
    }
    // returning int but shall cast back to uint
    return (int) orig_size;
}

int sys_print(trapframe* r)
{
    char* str = (char*) *(uint32_t*) (r->esp + 4);
    printf("%s", str);
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
    return fork();
}

void sys_exit(trapframe* r)
{
    int32_t exit_code = *(int32_t*) (r->esp + 4);
    printf("PID %u exiting with code %d\n", curr_proc()->pid, exit_code);
    exit(exit_code);
}

int sys_wait(trapframe* r)
{
    int* wait_status =  *(int**) (r->esp + 4);
    return wait(wait_status);
}

int sys_open(trapframe* r)
{
    char* path = *(char**) (r->esp + 4);
    int32_t flags = *(int*) (r->esp + 8);
    return fs_open(path, flags);
}

int sys_close(trapframe* r)
{
    int32_t fd = *(int*) (r->esp + 4);
    return fs_close(fd);
}

int sys_read(trapframe* r)
{
    int32_t fd = *(int*) (r->esp + 4);
    void* buf = *(void**) (r->esp + 8);
    uint32_t size = *(uint32_t*) (r->esp + 12);
    return fs_read(fd, buf, size);
}

int sys_write(trapframe* r)
{
    int32_t fd = *(int*) (r->esp + 4);
    void* buf = *(void**) (r->esp + 8);
    uint32_t size = *(uint32_t*) (r->esp + 12);
    return fs_write(fd, buf, size);
}

int sys_seek(trapframe* r)
{
    int32_t fd = *(int*) (r->esp + 4);
    int32_t offset = *(int32_t*) (r->esp + 8);
    int32_t whence = *(int32_t*) (r->esp + 12);
    return fs_seek(fd, offset, whence);
}

int sys_dup(trapframe* r)
{
    int32_t fd = *(int*) (r->esp + 4);
    return fs_dup(fd);
}

int sys_getattr_path(trapframe* r)
{
    char* path = *(char**) (r->esp + 4);
    struct fs_stat* st = *(struct fs_stat**) (r->esp + 8);
    return fs_getattr_path(path, st);
}

int sys_getattr_fd(trapframe* r)
{
    int fd = *(int*) (r->esp + 4);
    struct fs_stat* st = *(struct fs_stat**) (r->esp + 8);
    return fs_getattr_fd(fd, st);
}

int sys_get_pid(trapframe* r)
{
    UNUSED_ARG(r);
    proc* p = curr_proc();
    return p->pid;
}

int sys_curr_date_time(trapframe* r)
{
    date_time* dt = *(date_time**) (r->esp + 4);
    *dt = current_datetime();
    return 0;
}

void syscall_handler(trapframe* r)
{
    // Avoid scheduling when in syscall/kernel space
    disable_interrupt();

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
        sys_exit(r); // SYS_EXIT shall not return
        PANIC("Returned to exited process\n");
        break;
    case SYS_WAIT:
        r->eax = sys_wait(r);
        break;
    case SYS_SBRK:
        r->eax = sys_sbrk(r);
        break;
    case SYS_OPEN:
        r->eax = sys_open(r);
        break;
    case SYS_CLOSE:
        r->eax = sys_close(r);
        break;
    case SYS_READ:
        r->eax = sys_read(r);
        break;
    case SYS_WRITE:
        r->eax = sys_write(r);
        break;
    case SYS_SEEK:
        r->eax = sys_seek(r);
        break;
    case SYS_DUP:
        r->eax = sys_dup(r);
        break;
    case SYS_GETATTR_PATH:
        r->eax = sys_getattr_path(r);
        break;
    case SYS_GETATTR_FD:
        r->eax = sys_getattr_fd(r);
        break;
    case SYS_GET_PID:
        r->eax = sys_get_pid(r);
        break;
    case SYS_CURR_DATE_TIME:
        r->eax = sys_curr_date_time(r);
        break;
    default:
        printf("Unrecognized Syscall: %d\n", r->eax);
        PANIC("Unrecognized Syscall");
        r->eax = -1;
        break;
    }
}

