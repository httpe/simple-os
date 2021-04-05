extern user_main
global call_user_main

section .text

call_user_main:
pop eax ; remove the fake eip pushed by SYS_EXEC
call user_main
; main has returned, eax is return value

; try do a system call with the returned value
; with eax being the return value from the user_main C function
int 88

; test running a privileged instruction
; shall generate a General Protection Fault
cli

ret
