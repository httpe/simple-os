; Code to run right after entering user space
; Do a system call to EXEC /init

SYS_EXEC equ 1
INT_SYSCALL equ 88

section .start_init         ; special linkage treatment, see linker.ld
start_init_begin:
push argv
push init_prog_path
push 0                      ; fake return PC
mov eax, SYS_EXEC
int INT_SYSCALL             

exit:                       ; EXEC shall not return, but if so
jmp $                       ; enter infinite loop, hang


align 4
init_prog_path: db "/boot/usr/bin/init.elf", 0

argv:
dd init_prog_path
dd 0

start_init_end:

