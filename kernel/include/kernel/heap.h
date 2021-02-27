#ifndef _KERNEL_HEAP_H
#define _KERNEL_HEAP_H

#include <stdint.h>
#include <stddef.h>

void initialize_kernel_heap();
void kfree(uint32_t vaddr);
void* kmalloc(size_t size);

#endif