#include <arch/i386/kernel/pic.h>

static uint16_t __pic_get_irq_reg(int ocw3);
// static uint16_t pic_get_irr(void);
static uint16_t pic_get_isr(void);

/* reinitialize the PIC controllers, giving them specified vector offsets
   rather than 8h and 70h, as configured by default

These bytes give the PIC:
    Its vector offset. (ICW2)
    Tell it how it is wired to master/slaves. (ICW3)
    Gives additional information about the environment. (ICW4)

arguments:
    offset1 - vector offset for master PIC
        vectors on the master become offset1..offset1+7
    offset2 - same for slave PIC: offset2..offset2+7
*/
void PIC_remap(int offset1, int offset2) {
    unsigned char a1, a2;

    a1 = inb(PIC1_DATA);                        // save masks
    a2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  // starts the initialization sequence (in cascade mode)
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC1_DATA, offset1);                 // ICW2: Master PIC vector offset
    io_wait();
    outb(PIC2_DATA, offset2);                 // ICW2: Slave PIC vector offset
    io_wait();
    outb(PIC1_DATA, 4);                       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    io_wait();
    outb(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    outb(PIC1_DATA, a1);   // restore saved masks.
    outb(PIC2_DATA, a2);
}

bool PIC_is_spurious_irq(unsigned char irq)
{
    if (irq == 7 || irq == 15) {
        // Check for Spurious IRQs
        uint16_t isr = pic_get_isr();
        if (!((isr >> irq) & 1)) {
            // If n-th bit of the In-Service Register (ISR) is not set, it is spurious
            if (irq <= 7) {
                // No need to send EOI for spurious IRQ7 
                return true;
            } else {
                // Need to send EOI to master PIC for spurious IRQ15
                outb(PIC1_COMMAND, PIC_EOI);
                return true;
            }
        }
    }

    return false;
    
}

// This is issued to the PIC chips at the end of an IRQ-based interrupt routine.
// Only after acknowledging EOI can the SAME IRQ triggers interrupt/ISR (if interrupt is enabled)
// Other IRQ is not affected by EOI, as long as interrupt is enabled they will trigger an (nested) interrupt
// Return: if the IRQ is a spurious one (See https://wiki.osdev.org/PIC#Handling_Spurious_IRQs)
void PIC_sendEOI(unsigned char irq) {
    // If the IRQ came from the Master PIC, it is sufficient to issue this command only to the Master PIC; 
    // however if the IRQ came from the Slave PIC, it is necessary to issue the command to both PIC chips.
    if (irq >= 8)
        outb(PIC2_COMMAND, PIC_EOI);

    outb(PIC1_COMMAND, PIC_EOI);
}

// The PIC has an internal register called the IMR, or the Interrupt Mask Register. 
// It is 8 bits wide. This register is a bitmap of the request lines going into the PIC. 
// When a bit is set, the PIC ignores the request and continues normal operation. 
// Note that setting the mask on a higher request line will not affect a lower line. 
// Masking IRQ2 will cause the Slave PIC to stop raising IRQs.
void IRQ_set_mask(unsigned char IRQline) {
    uint16_t port;
    uint8_t value;

    if (IRQline < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        IRQline -= 8;
    }
    value = inb(port) | (1 << IRQline);
    outb(port, value);
}

void IRQ_clear_mask(unsigned char IRQline) {
    uint16_t port;
    uint8_t value;

    if (IRQline < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        IRQline -= 8;
    }
    value = inb(port) & ~(1 << IRQline);
    outb(port, value);
}


/* Helper func */
static uint16_t __pic_get_irq_reg(int ocw3) {
    /* OCW3 to PIC CMD to get the register values.  PIC2 is chained, and
     * represents IRQs 8-15.  PIC1 is IRQs 0-7, with 2 being the chain */
    outb(PIC1_COMMAND, ocw3);
    outb(PIC2_COMMAND, ocw3);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

/* Returns the combined value of the cascaded PICs irq request register */
// Not used, commenting out
// static uint16_t pic_get_irr(void) {
//     return __pic_get_irq_reg(PIC_READ_IRR);
// }

/* Returns the combined value of the cascaded PICs in-service register */
static uint16_t pic_get_isr(void) {
    return __pic_get_irq_reg(PIC_READ_ISR);
}
