#ifndef _KERNEL_HEAP_H
#define _KERNEL_HEAP_H

#include <stdint.h>
#include <stddef.h>

void initialize_kernel_heap();
void kfree(void* vaddr);
void* kmalloc(size_t size);

#endif