#ifndef _KERNEL_SYSCALL_H
#define _KERNEL_SYSCALL_H

// Syscall int number
#define INT_SYSCALL 88

#define SYS_EXEC 1
#define SYS_PRINT 2
#define SYS_YIELD 3
#define SYS_FORK 4
#define SYS_EXIT 5
#define SYS_WAIT 6
#define SYS_SBRK 7

// defined in interrupt.asm
extern void int88();

// arch specific, defined in isr.h
struct trapframe;

void syscall_handler(struct trapframe* r);

#endif