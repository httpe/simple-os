format	ELF executable 3
entry	start
segment readable executable

start:
    ; sys_test
    mov eax, 0
    push 4
    push 3
    push 2
    push 1
    ; dummy return address to mimic a function call
    ; i.e. (push eip + jmp) + (pop + jmp) = call + ret
    push 0
    int 88
    sub esp, 4*5

    ; exit
    mov eax, 5
    push 99
    push 0
    int 88
