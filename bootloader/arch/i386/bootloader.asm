; Modified from https://github.com/cfenollosa/os-tutorial/blob/master/13-kernel-barebones/bootsect.asm
; nasm -f bin -i arch/i386 -o mbr.bin arch/i386/mbr.asm

[org 0x7c00] ; boot loader offset fixed by BIOS convention

    ; set up the stack
    ; use conventional memory right before the boot loader (0x00000500 to 0x00007BFF, almost 30KiB)
    ; See https://wiki.osdev.org/Memory_Map_(x86)
    ; Using this address suggested by https://wiki.osdev.org/My_Bootloader_Does_Not_Work#There.27s_no_reference_to_SS_or_SP
    ; KB = 1000 bytes, KiB = 1024 bytes
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov bp, 0x7c00
    mov sp, bp

    ; print a hello world message
    mov bx, MSG_REAL_MODE
    call print ; This will be written after the BIOS messages
    call print_nl

    ; the boot load is assume to have 4 sector, load the other three here
    ; load right after this MBR sector
    ; real mode addressing is segment*16 + offset, so the 512 bytes MBR take 0x200 bytes
    ; and thus we choose to load from disk to 0x7c20:0, i.e. starting at 0x7c200
    mov dh, 3       
    mov bx, 0x7e00
    call disk_load  ; dl shall be loaded automatically during system boot to point to the current disk/media of this MBR

    mov bx, MSG_LOAD_FROM_DISK
    call print
    call print_nl

    ; ensure the disk is loaded successfully
    ; mov dx, [0x7e00]
    ; call print_hex

    call switch_to_pm
    jmp $       ; infinite loop, shall not reach here


MSG_REAL_MODE db "Started in 16-bit real mode", 0
MSG_LOAD_FROM_DISK db "Loaded remaining sectors", 0

; 16-bit printing utility functions
; symbol provided: print, print_nl
%include "print.asm"
; symbol provided: print_hex
%include "print_hex.asm"
; utility to read from disk by BIOS interrupt
%include "disk.asm"


; MBR shall be exactly 512 bytes (one sector)
times 510-($-$$) db 0
; MBR magic number
dw 0xaa55


; utility to enable A20 address line
; symbol provided: enable_a20
%include "a20.asm"
; utility to switch to 32-bit protected mode
; symbol provided: switch_to_pm
; symbol required: BEGIN_PM
%include "gdt.asm"
%include "switch_pm.asm"

[bits 32]
BEGIN_PM: ; after the switch we will get here
    mov esi, MSG_PROT_MODE
    mov edi, 14*80 ; write to the start of the 14th row
    mov ah, WHITE_ON_BLACK
    call print_pm ; Note that this will be written at the top left corner

    ; mov dl, 0x34
    ; call byte_to_hex_string
    ; mov word [0x00100000], dx
    ; mov byte [0x00100002], 0
    ; mov edi, 15*80
    ; mov esi, 0x00100000
    ; call print_pm
    ; jmp $

    ; test print_memory_hex
    mov esi, 0x7c00
    mov ecx, 32
    mov ah, WHITE_ON_BLACK
    mov edi, 15*80              ; print at the start of 15th row
    call print_memory_hex       ; shall print out '34 12 '

    jmp $                       ; infinite loop, hang

MSG_PROT_MODE db "Started in 32-bit protected mode", 0

; 32-bit protected mode print
; symbol provided: print_pm, WHITE_ON_BLACK, print_memory_hex
%include "print_pm.asm"


; The boot loader shall be exactly 2048 bytes (four sectors in total, one MBR sector and three utility sectors)
times 2048-($-$$) db 0