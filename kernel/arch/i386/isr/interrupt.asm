; Source: https://github.com/cfenollosa/os-tutorial/blob/master/23-fixes/cpu/interrupt.asm
; Ref: xv6/trapasm.S

; Defined in isr.c
[extern int_handler]

; kernel data segment descriptor in flat mode
KERNEL_DATA_SEG equ 0000000000010_0_00b

global int_ret

; Common code
common_stub:
    ; 1. Save CPU state
    
    ; save the segment descriptors
    push ds
    push es
    push fs
    push gs
    pusha ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

    ; switch to kernel code segment
	mov ax, KERNEL_DATA_SEG 
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

    ; 2. Call C handler
    push esp ; registers_t *r
    cld ; C code following the sysV ABI requires DF to be clear on function entry
	call int_handler
    add esp, 4 ; reverse 'push esp'

int_ret:
    ; 3. Restore state (also be used independently to enter user space)
    popa
    pop gs
    pop fs
    pop es
    pop ds

	add esp, 8 ; Cleans up the pushed error code and pushed ISR/IRQ number
	iret ; pops 5 things at once: EIP, CS, EFLAGS, ESP and SS (from low to high memory address)

	
; We don't get information about which interrupt was caller
; when the handler is run, so we will need to have a different handler
; for every interrupt.
; Furthermore, some interrupts push an error code onto the stack but others
; don't, so we will push a dummy error code for those which don't, so that
; we have a consistent stack for all of them.

; First make the ISRs global
global isr0
global isr1
global isr2
global isr3
global isr4
global isr5
global isr6
global isr7
global isr8
global isr9
global isr10
global isr11
global isr12
global isr13
global isr14
global isr15
global isr16
global isr17
global isr18
global isr19
global isr20
global isr21
global isr22
global isr23
global isr24
global isr25
global isr26
global isr27
global isr28
global isr29
global isr30
global isr31
; IRQs
global irq0
global irq1
global irq2
global irq3
global irq4
global irq5
global irq6
global irq7
global irq8
global irq9
global irq10
global irq11
global irq12
global irq13
global irq14
global irq15
; Syscall
global int88

; More on CPU exceptions: https://wiki.osdev.org/Exceptions
; 0: Divide By Zero Exception
isr0:
    push byte 0
    push byte 0
    jmp common_stub

; 1: Debug
isr1:
    push byte 0
    push byte 1
    jmp common_stub

; 2: Non Maskable Interrupt
isr2:
    push byte 0
    push byte 2
    jmp common_stub

; 3: Breakpoint
isr3:
    push byte 0
    push byte 3
    jmp common_stub

; 4: Overflow
isr4:
    push byte 0
    push byte 4
    jmp common_stub

; 5: Out of Bounds Exception
isr5:
    push byte 0
    push byte 5
    jmp common_stub

; 6: Invalid Opcode Exception
isr6:
    push byte 0
    push byte 6
    jmp common_stub

; 7: Device Not Available Exception
isr7:
    push byte 0
    push byte 7
    jmp common_stub

; 8: Double Fault Exception (With Error Code!)
isr8:
    push byte 8
    jmp common_stub

; 9: Coprocessor Segment Overrun Exception
isr9:
    push byte 0
    push byte 9
    jmp common_stub

; 10: Bad TSS Exception (With Error Code!)
isr10:
    push byte 10
    jmp common_stub

; 11: Segment Not Present Exception (With Error Code!)
isr11:
    push byte 11
    jmp common_stub

; 12: Stack Fault Exception (With Error Code!)
isr12:
    push byte 12
    jmp common_stub

; 13: General Protection Fault Exception (With Error Code!)
isr13:
    push byte 13
    jmp common_stub

; 14: Page Fault Exception (With Error Code!)
isr14:
    push byte 14
    jmp common_stub

; 15: Reserved Exception
isr15:
    push byte 0
    push byte 15
    jmp common_stub

; 16: x87 Floating-Point Exception
isr16:
    push byte 0
    push byte 16
    jmp common_stub

; 17: Alignment Check Exception (With Error Code!)
isr17:
    push byte 17
    jmp common_stub

; 18: Machine Check Exception
isr18:
    push byte 0
    push byte 18
    jmp common_stub

; 19: SIMD Floating-Point Exception
isr19:
    push byte 0
    push byte 19
    jmp common_stub

; 20: Virtualization Exception
isr20:
    push byte 0
    push byte 20
    jmp common_stub

; 21: Reserved
isr21:
    push byte 0
    push byte 21
    jmp common_stub

; 22: Reserved
isr22:
    push byte 0
    push byte 22
    jmp common_stub

; 23: Reserved
isr23:
    push byte 0
    push byte 23
    jmp common_stub

; 24: Reserved
isr24:
    push byte 0
    push byte 24
    jmp common_stub

; 25: Reserved
isr25:
    push byte 0
    push byte 25
    jmp common_stub

; 26: Reserved
isr26:
    push byte 0
    push byte 26
    jmp common_stub

; 27: Reserved
isr27:
    push byte 0
    push byte 27
    jmp common_stub

; 28: Reserved
isr28:
    push byte 0
    push byte 28
    jmp common_stub

; 29: Reserved
isr29:
    push byte 0
    push byte 29
    jmp common_stub

; 30: Security Exception (With Error Code!)
isr30:
    push byte 30
    jmp common_stub

; 31: Reserved
isr31:
    push byte 0
    push byte 31
    jmp common_stub

; IRQ handlers
; IRQs are remapped to interrupt 32-47 or 0x20-0x2F
; No error code for IRQs, instead we push the IRQ numbers to stack 
irq0:
	push byte 0
	push byte 32
	jmp common_stub

irq1:
	push byte 1
	push byte 33
	jmp common_stub

irq2:
	push byte 2
	push byte 34
	jmp common_stub

irq3:
	push byte 3
	push byte 35
	jmp common_stub

irq4:
	push byte 4
	push byte 36
	jmp common_stub

irq5:
	push byte 5
	push byte 37
	jmp common_stub

irq6:
	push byte 6
	push byte 38
	jmp common_stub

irq7:
	push byte 7
	push byte 39
	jmp common_stub

irq8:
	push byte 8
	push byte 40
	jmp common_stub

irq9:
	push byte 9
	push byte 41
	jmp common_stub

irq10:
	push byte 10
	push byte 42
	jmp common_stub

irq11:
	push byte 11
	push byte 43
	jmp common_stub

irq12:
	push byte 12
	push byte 44
	jmp common_stub

irq13:
	push byte 13
	push byte 45
	jmp common_stub

irq14:
	push byte 14
	push byte 46
	jmp common_stub

irq15:
	push byte 15
	push byte 47
	jmp common_stub

int88:
	push byte 0
	push byte 88
	jmp common_stub