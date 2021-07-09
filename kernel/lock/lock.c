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
    int pid = p?p->pid:0;

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
    // push_cli();
    acquire(&lk->lk);
    proc* p = curr_proc();
    PANIC_ASSERT(p != NULL);
    while(lk->writing_pid || lk->reading) {
        if(lk->writing_pid == p->pid) {
            PANIC("RW Write Dead Lock");
        }
        release(&lk->lk);
        yield();
        acquire(&lk->lk);
    }
    lk->writing_pid = p->pid;
    release(&lk->lk);
    // pop_cli();
}

void finish_writing(rw_lock* lk) 
{
    acquire(&lk->lk);
    PANIC_ASSERT(lk->writing_pid);
    lk->writing_pid = 0;
    release(&lk->lk);
}

void start_reading(rw_lock* lk) 
{
    acquire(&lk->lk);
    proc* p = curr_proc();
    PANIC_ASSERT(p != NULL);
    while(lk->writing_pid && lk->writing_pid != p->pid) {
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


