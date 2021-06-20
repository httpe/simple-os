; extern BEGIN_PM, do_e820
; global switch_to_pm

[bits 16]
switch_to_pm:

    ; Detect memory layout using BIOS interrupt 0x15, eax=0xE820 
    ; The entry count will be stored at 0x0500
    ; The actual memory layout/map entries will be stored starting at 0x0504
    call do_e820
    jnc e820_success
    mov bx, MSG_E820_FAILED
    call print
    jmp $
e820_success:
    ; make sure there are less than 170 segments found
    ; so the memory after 
    ; ADDR_MMAP_COUNT + 0x1000 > ADDR_MMAP_COUNT + 170 * 24 
    ; will be free of use
    cmp bp, 170
    jl e820_size_ok
    mov bx, MSG_E820_FAILED_TOO_MANY_SEG
    call print
    jmp $
e820_size_ok:
    mov bx, MSG_E820_SUCESS0
    call print
    mov dx, bp
    call print_hex
    mov bx, MSG_E820_SUCESS1
    call print
    call print_nl


    mov bx, MSG_SWITCH_PM_START
    call print
    call print_nl

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

    mov bx, MSG_SWITCH_PM_NMI
    call print
    call print_nl

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
    mov bx, MSG_A20_ENABLED
    call print
    call print_nl

    ; 3. Load the Global Descriptor Table with segment descriptors suitable for code, data, and stack
    cli                     ; 1. disable interrupts
    lgdt [gdt_descriptor]   ; 2. load the GDT descriptor
    mov eax, cr0
    or eax, 0x1             ; 3. set 32-bit mode bit (Protected Mode Enable	bit) in cr0 (https://en.wikipedia.org/wiki/Control_register)
    mov cr0, eax
    jmp CODE_SEG:init_pm    ; 4. far jump by using a different segment, basically setting CS to point to the correct protected mode segment (GDT entry)

; move cx bytes from es:si to es:di
memcpy:
    pusha
    rep movsb
    popa
    ret

; compare cx bytes from es:si and es:di
; set comparison flags accordingly
memcmp:
    pusha
    ; repe: rep while equal
    repe cmpsb
    popa
    ret
    


MSG_SWITCH_PM_START db "Start switching to Protected_Mode", 0
MSG_SWITCH_PM_NMI db "NMI Disabled", 0
MSG_A20_ENABLED db "A20 enabled", 0
MSG_A20_ENABLE_FAILED db "Enabling A20 failed, hanged", 0
MSG_E820_FAILED db "E820 memory detection failed, hanged", 0
MSG_E820_FAILED_TOO_MANY_SEG db "E820 detected too many segments, hanged", 0
MSG_E820_SUCESS0 db "E820 memory detection success with ", 0
MSG_E820_SUCESS1 db " memory segments", 0
STR_VBE2 db "VBE2"
STR_VESA db "VESA"
MSG_VESA_VERSION db "VESA version: ", 0
MSG_VESA_SUCCESS db "VESA success!", 0
MSG_VESA_FAILED db "VESA failed!", 0
MSG_VESA_VERIFY_FAILED db "VESA verify failed!", 0


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

