; section .text
; global _start:function (_start_end - _start)
; _start:
; 	extern _init			; https://wiki.osdev.org/Calling_Global_Constructors
;     call _init
 
; 	extern user_main
; 	call user_main
; _start_end:

USER_CODE_SEG equ 0000000000011_0_11b; 0x1b
USER_DATA_SEG equ 0000000000100_0_11b; 0x23

section .bss align=4096
align 16
stack_bottom:
resb 4096 ; 4 KiB
stack_top:

extern user_main

section .text
global _start
_start:

; Ref: https://littleosbook.github.io/#user-mode
; [esp + 16]  ss      ; the stack segment selector we want for user mode
; [esp + 12]  esp     ; the user mode stack pointer
; [esp +  8]  eflags  ; the control flags we want to use in user mode
; [esp +  4]  cs      ; the code segment selector
; [esp +  0]  eip     ; the instruction pointer of user mode code to execute
push USER_DATA_SEG
push stack_top
push 0x00000200       ; Interrupt Enabled
push USER_CODE_SEG
push _user_start
iret

_user_start:

mov ax, ss
mov ds, ax
mov es, ax

mov eax, esp
mov esp, stack_top ; switch to user stack
push eax ; save kernel stack esp
push ebp

push 2 ; int arguments
push 1
; push argv
; push argc

call user_main
; main has returned, eax is return value

add esp, 8

pop ebp
pop esp ; restore kernel stack esp

int 1

ret

; export PATH=~/opt/cross/bin/:$PATH
; nasm -f elf32 -g -F dwarf -o user_start.o user_start.asm
; i686-elf-ld -o test32.elf test32.o
; i686-elf-gcc -o user_start.elf -O0 -g -ffreestanding -Wall -Wextra -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs user_start.o
; i686-elf-gcc --sysroot=/mnt/e/Project-OS/src/simple-os/sysroot -isystem=/usr/include -o user_start.elf -O0 -g -ffreestanding -Wall -Wextra -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs user_start.o
; i686-elf-gcc --sysroot=/mnt/e/Project-OS/src/simple-os/sysroot -isystem=/usr/include -MD -c user.c -o user.o -std=gnu11 -O0 -g -ffreestanding -Wall -Wextra -D__is_kernel -Iinclude
; i686-elf-gcc --sysroot=/mnt/e/Project-OS/src/simple-os/sysroot -isystem=/usr/include -o user.elf -O0 -g -ffreestanding -Wall -Wextra -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs user_start.o user.o