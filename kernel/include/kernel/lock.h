#ifndef _KERNEL_LOCK_H
#define _KERNEL_LOCK_H

#include <stdint.h>
#include <common.h>

// Disabling interrupt when locked, strictest
typedef struct lock {
    uint locked;
} lock;

// Use this ONLY if the resoure will NOT be used in any interrupt handler
// otherwise a deallock can happen, since the lock rely on yield() to wait
// the lock holder to release to lock, but we cannot yield to the same process
// just not in the interrupt handler
typedef struct rw_lock {
    lock lk;
    uint writing;
    uint reading;
} rw_lock;


void acquire(lock* lk);
void release(lock* lk);

void start_writing(rw_lock* lk);
void finish_writing(rw_lock* lk);
void start_reading(rw_lock* lk);
void finish_reading(rw_lock* lk);

#endif