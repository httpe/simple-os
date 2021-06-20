; Modified from https://github.com/cfenollosa/os-tutorial/blob/master/13-kernel-barebones/bootsect.asm

; magic number to check after loading the non-MBR sectors from disk
DISK_LOAD_MAGIC equ 0x1234

section .boot
; Note that .boot section will not generate debug symbol
;   so you cannot debug this section in gdb without using absolute address
;   e.g. gdb command: "break *0x7c00"
[bits 16]
global boot:function
boot:
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

    ; the boot loader is assumed to have 16 sectors, load the other 15 sectors here
    ; This file will yield 4 sectors (the assembly part), and the C part will yield the other 12 sectors
    ; load to right after this MBR sector
    ; real mode addressing is segment*16 + offset, so the 512 bytes MBR take 0x200 bytes
    ; and thus we choose to load from disk to 0:0x7e00
    ; must be in sync of BOOTLOADER_MAX_SIZE in Makefile
    mov dh, 15
    mov bx, 0x7e00
    call disk_load  ; dl shall be loaded automatically during system boot to point to the current disk/media of this MBR

    ; ensure the disk is loaded successfully by comparing the magic number
    mov dx, [0x7e00]
    cmp dx, DISK_LOAD_MAGIC
    je disk_load_success
    mov bx, MSG_LOAD_FROM_DISK_FAILED
    call print
    jmp $
disk_load_success:
    mov bx, MSG_LOAD_FROM_DISK
    call print
    call print_nl

    call switch_to_pm
    jmp $       ; infinite loop, shall not reach here


MSG_REAL_MODE db "Started in 16-bit real mode", 0
MSG_LOAD_FROM_DISK db "Remaining sectors loaded successfully", 0
MSG_LOAD_FROM_DISK_FAILED db "Failed loading remaining sectors, hanged", 0

; 16-bit printing utility functions
; symbol provided: print, print_nl
%include "print.asm"
; symbol provided: print_hex
%include "print_hex.asm"
; utility to read from disk by BIOS interrupt
%include "disk.asm"


; MBR shall be exactly 512 bytes (one sector)
; $ current line;
; $$: currect section;
times 510-($-$$) db 0 ; padding 510 - (279 - 0) = 231's 0
; MBR magic number
dw 0xaa55 ; write 0xaa55 in 511 and 512 bytes


; switch to .text section so the symbol are exported
section .text

; first two magic bytes of the first sector to be loaded from disk dynamically 
dw DISK_LOAD_MAGIC

; utility to detect memory layout
%include "memory.asm"
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
    ; jmp $: should print highlighted Started in 32-bit protected mode

    ; test print_memory_hex
    mov esi, 0x7c00 + 0x1FE
    mov ecx, 2
    mov ah, WHITE_ON_BLACK
    mov edi, 15*80              ; print at the start of 15th row
    call print_memory_hex       ; shall print out the MBR magic number '55 AA '

    ; start switching to C compiled code
    extern bootloader_main
    call bootloader_main

    jmp $                       ; infinite loop, hang

MSG_PROT_MODE db "Started in 32-bit protected mode", 0

; 32-bit protected mode print
; symbol provided: print_pm, WHITE_ON_BLACK, print_memory_hex
%include "print_pm.asm"


; The boot loader shall be exactly 2048 bytes (four sectors in total, one MBR sector and three utility sectors)
times 2048-($-$$) db 0