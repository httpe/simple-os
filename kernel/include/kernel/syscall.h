#ifndef _KERNEL_SYSCALL_H
#define _KERNEL_SYSCALL_H

// Syscall int number
#define INT_SYSCALL 88

// defined in interrupt.asm
extern void int88();

// arch specific, defined in isr.h
struct trapframe;

void syscall_handler(struct trapframe* r);

#endif