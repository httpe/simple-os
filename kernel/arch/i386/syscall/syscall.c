#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syscall.h>
#include <common.h>
#include <network.h>
#include <kernel/errno.h>
#include <kernel/panic.h>
#include <kernel/vfs.h>
#include <kernel/process.h>
#include <kernel/ethernet.h>
#include <kernel/ipv4.h>
#include <kernel/icmp.h>
#include <kernel/cpu.h>
#include <kernel/video.h>
#include <arch/i386/kernel/isr.h>

int sys_exec(trapframe* r)
{
    // TODO: parameter security check
    // uint32_t num = *(uint32_t*) r->esp;
    char* path = (char*) *(uint32_t*) (r->esp + 4);
    char** argv = (char**) *(uint32_t*) (r->esp + 8);
    // printf("SYS_EXEC: eip: %u, path: %s\n", num, path);
    return exec(path, argv);
}

// increase/decrease process image size
int sys_sbrk(trapframe* r)
{
    int32_t delta = *(int32_t*) (r->esp + 4);
    
    proc* p = curr_proc();
    uint32_t orig_size = p->size;
    uint32_t new_size = p->size + delta;
    if(new_size < p->orig_size) {
        return -EINVAL;
    } 
    p->size = new_size;
    uint32_t orig_last_pg_idx = PAGE_INDEX_FROM_VADDR(orig_size - 1);
    uint32_t new_last_pg_idx =  PAGE_INDEX_FROM_VADDR(new_size - 1);
    if(new_last_pg_idx > orig_last_pg_idx) {
        alloc_pages_at(p->page_dir, orig_last_pg_idx + 1, new_last_pg_idx - orig_last_pg_idx, false, true);
    } else if(new_last_pg_idx < orig_last_pg_idx) {
        dealloc_pages(p->page_dir, new_last_pg_idx + 1, orig_last_pg_idx - new_last_pg_idx); 
    }
    // if(delta > 0) {
    //     memset((void*) orig_size, 0, delta);
    // }
    // PANIC_ASSERT(is_vaddr_accessible(p->page_dir, orig_size, false, true));
    // if(delta > 0) {
    //     PANIC_ASSERT(is_vaddr_accessible(p->page_dir, orig_size + delta - 1, false, true));
    //     if(delta > PAGE_SIZE) {
    //         PANIC_ASSERT(is_vaddr_accessible(p->page_dir, orig_size + PAGE_SIZE, false, true));
    //     }
    // }
    // PANIC_ASSERT(!is_vaddr_accessible(p->page_dir, 0xEFFFFFFF, false, true));
    
    // uint32_t p1 = vaddr2paddr(p->page_dir, 0x805af95);
    // uint32_t p2 = vaddr2paddr(p->page_dir, 0xbfffff94);

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
    // printf("PID %u exiting with code %d\n", curr_proc()->pid, exit_code);
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
    char* abs_path = get_abs_path(path);
    if(abs_path == NULL) {
        return -ENOENT;
    }
    int res = fs_open(abs_path, flags);
    free(abs_path);
    return res;
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
    char* abs_path = get_abs_path(path);
    if(abs_path == NULL) {
        return -ENOENT;
    }
    int res = fs_getattr_path(abs_path, st);
    free(abs_path);
    return res;
}

int sys_getattr_fd(trapframe* r)
{
    int fd = *(int*) (r->esp + 4);
    struct fs_stat* st = *(struct fs_stat**) (r->esp + 8);
    return fs_getattr_fd(fd, st);
}

int sys_truncate_path(trapframe* r)
{
    char* path = *(char**) (r->esp + 4);
    uint size = *(uint*) (r->esp + 8);
    char* abs_path = get_abs_path(path);
    if(abs_path == NULL) {
        return -ENOENT;
    }
    int res = fs_truncate_path(abs_path, size);
    free(abs_path);
    return res;
}

int sys_truncate_fd(trapframe* r)
{
    int fd = *(int*) (r->esp + 4);
    uint size = *(uint*) (r->esp + 8);
    return fs_truncate_fd(fd, size);
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

int sys_unlink(trapframe* r)
{
    char* path = *(char**) (r->esp + 4);
    char* abs_path = get_abs_path(path);
    if(abs_path == NULL) {
        return -ENOENT;
    }
    int res = fs_unlink(abs_path);
    free(abs_path);
    return res;
}

int sys_link(trapframe* r)
{
    char* old_path = *(char**) (r->esp + 4);
    char* new_path = *(char**) (r->esp + 8);

    char* old_abs_path = get_abs_path(old_path);
    if(old_abs_path == NULL) {
        return -ENOENT;
    }
    char* new_abs_path = get_abs_path(new_path);
    if(new_abs_path == NULL) {
        return -ENOENT;
    }

    int res = fs_link(old_abs_path, new_abs_path);

    free(old_abs_path);
    free(new_abs_path);

    return res;
}

int sys_rename(trapframe* r)
{
    char* old_path = *(char**) (r->esp + 4);
    char* new_path = *(char**) (r->esp + 8);

    char* old_abs_path = get_abs_path(old_path);
    if(old_abs_path == NULL) {
        return -ENOENT;
    }
    char* new_abs_path = get_abs_path(new_path);
    if(new_abs_path == NULL) {
        return -ENOENT;
    }

    uint flags = *(uint*) (r->esp + 12);
    int res = fs_rename(old_abs_path, new_abs_path, flags);

    free(old_abs_path);
    free(new_abs_path);

    return res;
}

int sys_readdir(trapframe* r)
{
    const char * path = *(const char**) (r->esp + 4);
    uint entry_offset = *(uint*) (r->esp + 8);
    fs_dirent* buf = *(fs_dirent**) (r->esp + 12);
    uint buf_size = *(uint *) (r->esp + 16);

    char* abs_path = get_abs_path(path);
    if(abs_path == NULL) {
        return -ENOENT;
    }
    
    int res = fs_readdir(abs_path, entry_offset, buf, buf_size);

    free(abs_path);
    return res;
}

int sys_chdir(trapframe* r)
{
    const char * path = *(const char**) (r->esp + 4);
    return chdir(path);
}

int sys_getcwd(trapframe* r)
{
    char * buf = *(char**) (r->esp + 4);
    size_t buf_size = *(size_t *) (r->esp + 8);
    return getcwd(buf, buf_size);
}

int sys_test(trapframe* r)
{
    UNUSED_ARG(r);

    printf("Halting...\n");

    while(1) {
        // process_IRQ(1);
        yield();
        PANIC_ASSERT(!is_interrupt_enabled());
    }    

    return 0;
}

int sys_mkdir(trapframe* r)
{
    const char* path = *(const char**) (r->esp + 4);
    char* abs_path = get_abs_path(path);
    if(abs_path == NULL) {
        return -ENOENT;
    }
    uint mode = *(uint*) (r->esp + 8);
    return fs_mkdir(abs_path, mode);
}

int sys_rmdir(trapframe* r)
{
    const char* path = *(const char**) (r->esp + 4);
    char* abs_path = get_abs_path(path);
    if(abs_path == NULL) {
        return -ENOENT;
    }
    return fs_rmdir(abs_path);
}

int sys_network_receive_ipv4_pkt(trapframe* r)
{
    char * buf = *(char**) (r->esp + 4);
    uint buf_size = *(uint *) (r->esp + 8);
    int res = ipv4_wait_for_next_packet(buf, buf_size);
    return res;
}

int sys_prep_icmp_pkt(trapframe* r) 
{
    icmp_opt * opt = *(icmp_opt**) (r->esp + 4);
    char * buf = *(char**) (r->esp + 8);
    uint buf_size = *(uint *) (r->esp + 12);
    int res = icmp_prep_pkt(opt, buf, buf_size);
    return res;
}

int sys_send_icmp_pkt(trapframe* r)
{
    icmp_opt * opt = *(icmp_opt**) (r->esp + 4);
    char * buf = *(char**) (r->esp + 8);
    uint pkt_len = *(uint *) (r->esp + 12);
    int res = icmp_send_pkt(opt, buf, pkt_len);
    return res;
}

int sys_refresh_screen(trapframe* r)
{
    UNUSED_ARG(r);
    video_refresh();
    return 0;
}

int sys_draw_picture(trapframe* r)
{
    uint32_t* buf = *(uint32_t**) (r->esp + 4);
    int x = *(int *) (r->esp + 8);
    int y = *(int *) (r->esp + 12);
    int w = *(int *) (r->esp + 16);
    int h = *(int *) (r->esp + 20);
    drawpic(buf, x, y, w, h);
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
    case SYS_TEST:
        r->eax = sys_test(r);
        break;
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
    case SYS_UNLINK:
        r->eax = sys_unlink(r);
        break;
    case SYS_LINK:
        r->eax = sys_link(r);
        break;
    case SYS_RENAME:
        r->eax = sys_rename(r);
        break;
    case SYS_READDIR:
        r->eax = sys_readdir(r);
        break;
    case SYS_CHDIR:
        r->eax = sys_chdir(r);
        break;
    case SYS_GETCWD:
        r->eax = sys_getcwd(r);
        break;
    case SYS_TRUNCATE_PATH:
        r->eax = sys_truncate_path(r);
        break;
    case SYS_TRUNCATE_FD:
        r->eax = sys_truncate_fd(r);
        break;
    case SYS_MKDIR:
        r->eax = sys_mkdir(r);
        break;
    case SYS_RMDIR:
        r->eax = sys_rmdir(r);
        break;
    case SYS_NETWORK_RECEIVE_IPv4_PKT:
        r->eax = sys_network_receive_ipv4_pkt(r);
        break;
    case SYS_REFRESH_SCREEN:
        r->eax = sys_refresh_screen(r);
        break;
    case SYS_DRAW_PICTURE:
        r->eax = sys_draw_picture(r);
        break;
    case SYS_PREP_ICMP_PKT:
        r->eax = sys_prep_icmp_pkt(r);
        break;
    case SYS_SEND_ICMP_PKT:
        r->eax = sys_send_icmp_pkt(r);
        break;
    default:
        printf("Unrecognized Syscall: %d\n", r->eax);
        PANIC("Unrecognized Syscall");
        r->eax = -1;
        break;
    }
}

