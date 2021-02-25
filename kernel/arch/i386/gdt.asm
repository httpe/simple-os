; global gdt_descriptor, CODE_SEG, DATA_SEG
; Based on https://github.com/cfenollosa/os-tutorial/tree/master/09-32bit-gdt

push CODE_SEG
push DATA_SEG

; Using Flat Setup in GDT, basically leaving the offsets used in addressing untranslated 
gdt_start: ; don't remove the labels, they're needed to compute sizes and jumps
    ; the GDT starts with a null 8-byte
    dd 0x0 ; 4 byte
    dd 0x0 ; 4 byte

; GDT for code segment. base = 0x00000000, length = 0xfffff
; for flags, refer to os-dev.pdf document, page 36
; Ref: https://wiki.osdev.org/Global_Descriptor_Table
gdt_code: 
    dw 0xffff    ; segment length, bits 0-15
    dw 0x0       ; segment base, bits 0-15
    db 0x0       ; segment base, bits 16-23
    db 1001_1010b ; flags (8 bits)
    db 1100_1111b ; flags (4 bits) + segment length, bits 16-19
    db 0x0       ; segment base, bits 24-31

; GDT for data segment. base and length identical to code segment
; some flags changed, again, refer to os-dev.pdf
gdt_data:
    dw 0xffff
    dw 0x0
    db 0x0
    db 1001_0010b
    db 1100_1111b
    db 0x0

; TODO: Add TSS segment descriptor here
gdt_user_code:
    dw 0xffff
    dw 0x0
    db 0x0
    db 1111_1010b
    db 1100_1111b
    db 0x0

gdt_user_data:
    dw 0xffff
    dw 0x0
    db 0x0
    db 1111_0010b
    db 1100_1111b
    db 0x0

gdt_end:



; GDT descriptor to be loaded by lgdt instruction
gdt_descriptor:
    dw gdt_end - gdt_start - 1 ; size (16 bit), always one less of its true size
    dd gdt_start ; address (32 bit)


; define some constants for later use
; these are segment selector used in protected mode
; Ref: https://wiki.osdev.org/Selector
; When Requested Privilege Level is 0 and we are using GDT, they are the same as the byte offset into the GDT table
; Since the GDT entry has a length of 8 bytes and and first two bits of selector is RPL, the third is to choose GDT (0b) or LDT (1b)
; and the remaining of the 16bits selector is the 0-based index into the GDT


CODE_SEG equ 0000000000001_0_00b ; gdt_code - gdt_start is also correct
DATA_SEG equ 0000000000010_0_00b ; gdt_data - gdt_start is also correct
USER_CODE_SEG equ 0000000000011_0_11b; 0x1b
USER_DATA_SEG equ 0000000000100_0_11b; 0x23
