; extern BEGIN_PM, do_e820
; global switch_to_pm

[bits 16]

switch_to_pm:

    ; mov bx, MSG_SWITCH_PM_START
    ; call print
    ; call print_nl

    ; start the procedure of entering real mode
    ; ref: https://wiki.osdev.org/Protected_Mode

    ; 1. Disable interrupts, including NMI (as suggested by Intel Developers Manual)
    ;  void NMI_disable() {
    ;     outb(0x70, inb(0x70) | 0x80);
    ;  }
    ; port 0x70 is the CMOS register selector, the highest bit controls the NMI, set means disabled and clear means enabled
    ; see https://wiki.osdev.org/CMOS#CMOS_Registers and https://wiki.osdev.org/I/O_Ports
    in al, 0x70
    or al, 1000_0000b
    out 0x70, al

    ; mov bx, MSG_SWITCH_PM_NMI
    ; call print
    ; call print_nl

    ; 2. Enable the A20 Line
    call enable_a20
    cmp ax, 1
    je .a20_enabled
.a20_enable_failed:
    mov bx, MSG_A20_ENABLE_FAILED
    call print
.a20_hang:
    jmp .a20_hang                       ; if failed in enabling A20, hang
.a20_enabled:
    ; mov bx, MSG_A20_ENABLED
    ; call print
    ; call print_nl

    ; 3. Load the Global Descriptor Table with segment descriptors suitable for code, data, and stack
    cli                     ; 1. disable interrupts
    lgdt [gdt_descriptor]   ; 2. load the GDT descriptor
    mov eax, cr0
    or eax, 0x1             ; 3. set 32-bit mode bit (Protected Mode Enable	bit) in cr0 (https://en.wikipedia.org/wiki/Control_register)
    mov cr0, eax
    jmp CODE_SEG:init_pm    ; 4. far jump by using a different segment, basically setting CS to point to the correct protected mode segment (GDT entry)



MSG_SWITCH_PM_START db "Start switching to Protected_Mode", 0
MSG_SWITCH_PM_NMI db "NMI Disabled", 0
MSG_A20_ENABLED db "A20 enabled", 0
MSG_A20_ENABLE_FAILED db "Enabling A20 failed, hanged", 0


[bits 32]
init_pm:                    ; we are now using 32-bit instructions
    mov ax, DATA_SEG        ; 5. update the segment registers other than CS
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax


    mov ebp, 0x00F00000     ; 6. update the stack right at the top of the free space, a 14MiB space above 1MiB. Ref: https://wiki.osdev.org/Memory_Map_(x86)
    mov esp, ebp

    call BEGIN_PM ; 7. Call a well-known label with useful code

