#ifndef _KERNEL_LOCK_H
#define _KERNEL_LOCK_H

#include <stdint.h>
#include <common.h>

// Disabling interrupt when locked, strictest
// When the lock is holding by other process, yield
// When the lock is holding by the same process, panic
// Under single-CPU setting, if no manual yielding in critical region,
//   it is suffice to protect only the write operation
typedef struct yield_lock {
    uint locked;
    uint holding_pid;
} yield_lock;

// Does NOT disable interrupt when locked
// Use this ONLY if the resoure will NOT be used in any interrupt handler
// otherwise a deallock can happen, since the lock rely on yield() to wait
// the writer to release the lock, but we cannot yield to the same process
// from the interrupt handler
typedef struct rw_lock {
    yield_lock lk;
    uint writing;
    uint reading;
} rw_lock;


void acquire(yield_lock* lk);
void release(yield_lock* lk);
uint holding(yield_lock* lk);

void start_writing(rw_lock* lk);
void finish_writing(rw_lock* lk);
void start_reading(rw_lock* lk);
void finish_reading(rw_lock* lk);

#endif