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
#define SYS_OPEN 8
#define SYS_CLOSE 9
#define SYS_READ 10
#define SYS_WRITE 11
#define SYS_SEEK 12
#define SYS_DUP 13

// defined in interrupt.asm
extern void int88();

// arch specific, defined in isr.h
struct trapframe;

void syscall_handler(struct trapframe* r);

#endif