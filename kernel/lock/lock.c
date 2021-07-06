#include <kernel/panic.h>
#include <kernel/process.h>
#include <kernel/lock.h>
#include <kernel/cpu.h>

// Ref: xv6/spinlock.c

void acquire(yield_lock* lk)
{
    // Assuming there is only one CPU, so disabling interrupt will be enough
    // to ensure atomicity of the check-and-flag code below
    // no need to use atomic assembly like xchg
    push_cli();

    proc* p = curr_proc();
    uint pid = p?p->pid:0;

    while(lk->locked) {
        if(lk->holding_pid == pid) {
            PANIC("Deal Lock");
        }
        
        yield();
    }
    lk->locked = 1;
    lk->holding_pid = pid;
    

    // if in multi-core scenario, need to insert 
    // __sync_synchronize()
    // here
}

void release(yield_lock* lk)
{
    // Assuming there is only one CPU, so disabling interrupt will be enough
    // to ensure atomicity of the check-and-flag code below
    // no need to use atomic assembly like xchg
    PANIC_ASSERT(!is_interrupt_enabled());
    if(!lk->locked) {
        PANIC("Releasing Non-holding lock");
    }
    
    // if in multi-core scenario, need to insert 
    // __sync_synchronize()
    // here
    lk->locked = 0;
    pop_cli();
}

uint holding(yield_lock* lk)
{
  push_cli();
  uint r = lk->locked;
  pop_cli();
  return r;
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


