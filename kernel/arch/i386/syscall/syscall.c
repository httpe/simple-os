#include <stdio.h>
#include <arch/i386/kernel/syscall.h>
#include <arch/i386/kernel/process.h>
#include <kernel/panic.h>

int sys_exec(trapframe* r)
{
    // TODO: parameter security check
    uint32_t num = *(uint32_t*) r->esp;
    char* path = (char*) *(uint32_t*) (r->esp + 4);
    char** argv = (char**) *(uint32_t*) (r->esp + 8);
    char* path1 = argv[0];
    printf("SYS_EXEC: %u, %s, %s\n", num, path, path1);
    printf("SYS_EXEC not yet implemented, returning -1\n");
    return -1;
}

void syscall_handler(trapframe* r)
{
    if(r->eax == SYS_EXEC) {
        // trapframe r will be pop when returning to user space
        // so it will be the return value of the syscall  
        r->eax = sys_exec(r);
    } else {
        printf("Unrecognized Syscall: %d\n", r->eax);
        r->eax = -1;
    }
}

