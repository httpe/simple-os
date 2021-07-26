#include <kernel/timer.h>
#include <kernel/time.h>
#include <kernel/process.h>
#include <kernel/lock.h>
#include <arch/i386/kernel/cpu.h>
#include <arch/i386/kernel/isr.h>
#include <arch/i386/kernel/port_io.h>
#include <stdio.h>
#include <common.h>

// Ref: http://www.jamesmolloy.co.uk/tutorial_html/5.-IRQs%20and%20the%20PIT.html
// Ref: https://github.com/cfenollosa/os-tutorial/blob/master/23-fixes/cpu/timer.c


static uint32_t tick_between_call_to_scheduler = 0;
static uint32_t timer_freq = 0;
static uint64_t tick = 0;
static time_t init_ts;

static void timer_callback(trapframe *regs) {
    UNUSED_ARG(regs);

    tick++;
    
    if(tick_between_call_to_scheduler > 0 && tick % tick_between_call_to_scheduler == 0) {
        yield();
    }

    // Calibrate tick at second precision
    if(tick % (timer_freq) == 0) {
        if(!init_ts) {
                date_time init_dt = current_datetime();
                init_ts = datetime2epoch(&init_dt);
        } else {
            date_time dt =  current_datetime();
            time_t ts = datetime2epoch(&dt);
            int64_t sec_elapsed = ts - init_ts;
            uint64_t tick_est = sec_elapsed * timer_freq;
            if(tick_est > tick) {
                printf("Timer: Calibrating tick, %llu => %llu\n", tick, tick_est);
                tick = tick_est;
            }
        }
    }
}


/*
Programmable Interval Timer Spec (https://wiki.osdev.org/PIT)

I/O port     Usage
0x40         Channel 0 data port (read/write)
0x41         Channel 1 data port (read/write)
0x42         Channel 2 data port (read/write)
0x43         Mode/Command register (write only, a read is ignored)

The Mode/Command register at I/O address 0x43 contains the following:
Bits         Usage
6 and 7      Select channel :
                0 0 = Channel 0
                0 1 = Channel 1
                1 0 = Channel 2
                1 1 = Read-back command (8254 only)
4 and 5      Access mode :
                0 0 = Latch count value command
                0 1 = Access mode: lobyte only
                1 0 = Access mode: hibyte only
                1 1 = Access mode: lobyte/hibyte
1 to 3       Operating mode :
                0 0 0 = Mode 0 (interrupt on terminal count)
                0 0 1 = Mode 1 (hardware re-triggerable one-shot)
                0 1 0 = Mode 2 (rate generator)
                0 1 1 = Mode 3 (square wave generator)
                1 0 0 = Mode 4 (software triggered strobe)
                1 0 1 = Mode 5 (hardware triggered strobe)
                1 1 0 = Mode 2 (rate generator, same as 010b)
                1 1 1 = Mode 3 (square wave generator, same as 011b)
0            BCD/Binary mode: 0 = 16-bit binary, 1 = four-digit BCD

*/

// Initialize timer (IRQ0 from Programmable Interval Timer)
void init_timer(uint32_t freq, uint32_t tick_between_process_switch) {
    /* Install the PIC interrupt handler */
    register_interrupt_handler(IRQ_TO_INTERRUPT(0), timer_callback);

    // The value we send to the PIT is the value to divide it's input clock
    // (1193180 Hz) by, to get our required frequency. Important to note is
    // that the divisor must be small enough to fit into 16-bits.
    uint32_t divisor = 1193180 / freq;
    uint8_t low  = (uint8_t)(divisor & 0x00FF);
    uint8_t high = (uint8_t)( (divisor >> 8) & 0x00FF);

    /* Send the command */
    outb(0x43, 0b00110110); /* Command port */
    outb(0x40, low);
    outb(0x40, high);

    timer_freq = freq;
    tick_between_call_to_scheduler = tick_between_process_switch;
}
