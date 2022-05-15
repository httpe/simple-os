#ifndef _KERNEL_TIME_H
#define _KERNEL_TIME_H

#include <stdint.h>
#include <kernel/types.h>
#include <datetime.h>

date_time current_datetime();
time_t datetime2epoch(date_time* tim_p);
int64_t cpu_freq();

#endif