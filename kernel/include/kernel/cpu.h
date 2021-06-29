#ifndef _KERNEL_CPU_H
#define _KERNEL_CPU_H

#include <common.h>

void disable_interrupt();
void enable_interrupt();
int is_interrupt_enabled();
void push_cli();
void pop_cli();
void halt();
// atomically exchange/swap value
uint xchg(volatile uint *addr, uint newval);

#endif