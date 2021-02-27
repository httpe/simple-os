#include <stdio.h>
#include <arch/i386/kernel/syscall.h>

void syscall_handler(trapframe* r)
{
    printf("Syscall: %d\n", r->eax);
}