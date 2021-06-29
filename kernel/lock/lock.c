#include <kernel/panic.h>
#include <kernel/process.h>
#include <kernel/lock.h>
#include <kernel/cpu.h>

// Ref: xv6/spinlock.c

void acquire(lock* lk)
{
    // Assuming there is only one CPU, so disabling interrupt will be enough
    // to ensure atomicity of the check-and-flag code below
    // no need to use atomic assembly like xchg
    push_cli();
    while(lk->locked) {
        yield();
    }
    lk->locked = 1;

    // if in multi-core scenario, need to insert 
    // __sync_synchronize()
    // here
}

void release(lock* lk)
{
    // Assuming there is only one CPU, so disabling interrupt will be enough
    // to ensure atomicity of the check-and-flag code below
    // no need to use atomic assembly like xchg
    PANIC_ASSERT(!is_interrupt_enabled());
    PANIC_ASSERT(lk->locked);
    // if in multi-core scenario, need to insert 
    // __sync_synchronize()
    // here
    lk->locked = 0;
    pop_cli();
}

void start_writing(rw_lock* lk) 
{
    disable_interrupt();
    acquire(&lk->lk);
    while(lk->writing || lk->reading) {
        release(&lk->lk);
        yield();
        acquire(&lk->lk);
    }
    lk->writing = 1;
    release(&lk->lk);
    enable_interrupt();
}

void finish_writing(rw_lock* lk) 
{
    acquire(&lk->lk);
    PANIC_ASSERT(lk->writing);
    lk->writing = 0;
    release(&lk->lk);
}

void start_reading(rw_lock* lk) 
{
    acquire(&lk->lk);
    while(lk->writing) {
        release(&lk->lk);
        yield();
        acquire(&lk->lk);
    }
    lk->reading++;
    release(&lk->lk);
}

void finish_reading(rw_lock* lk) 
{
    acquire(&lk->lk);
    PANIC_ASSERT(lk->reading > 0);
    lk->reading--;
    release(&lk->lk);
}


