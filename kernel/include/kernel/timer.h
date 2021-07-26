#ifndef _KERNEL_TIMER_H
#define _KERNEL_TIMER_H

#include <stdint.h>

void init_timer(uint32_t freq, uint32_t tick_between_process_switch);

#endif