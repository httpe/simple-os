#ifndef _ARCH_I386__KERNEL_SYSCALL_H
#define _ARCH_I386__KERNEL_SYSCALL_H

#include <arch/i386/kernel/isr.h>

// defined in interrupt.asm
extern void int88();

void syscall_handler(trapframe* r);

#endif