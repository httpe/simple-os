#ifndef _KERNEL_ARCH_INIT_H
#define _KERNEL_ARCH_INIT_H

#include <stdint.h>

// Architecture specific initialization
void initialize_architecture(uint32_t mbt_physical_addr);

#endif