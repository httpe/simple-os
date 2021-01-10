; global enable_a20
; Check and Enable A20 address line in x86 machine
; from https://wiki.osdev.org/A20_Line

; The following code is public domain licensed
 
[bits 16]

; Try different methods to enable A20 address line
; ax = 1 if success
; ax = 0 if failed
; Test if A20 is already enabled - if it is you don't need to do anything at all
; Try the BIOS function. Ignore the returned status.
; Test if A20 is enabled (to see if the BIOS function actually worked or not)
; Try the keyboard controller method.
; Test if A20 is enabled in a loop with a time-out (as the keyboard controller method may work slowly)
; Try the Fast A20 method last
; Test if A20 is enabled in a loop with a time-out (as the fast A20 method may work slowly)
; If none of the above worked, give up
enable_a20:
    pushf
    pusha

    call check_a20
    cmp ax, 0
    jnz enable_a20_done

    call enable_a20_bios
    call check_a20
    cmp ax, 0
    jnz enable_a20_done

    call enable_a20_keyboard
.loop1:
    mov cx, 10                  ; Time-out after 10 tests
    call a20wait                ; Add delay
    call check_a20
    cmp ax, 0
    jnz enable_a20_done
    loop .loop1

    call enable_a20_fast
.loop2:
    mov cx, 10                  ; Time-out after 10 tests
    call check_a20              ; Add delay
    cmp ax, 0
    jnz enable_a20_done
    loop .loop2

enable_a20_failed:
    popa
    popf
    mov ax, 0
    ret
enable_a20_done:
    popa
    popf
    mov ax, 1
    ret


; Function: check_a20
;
; Purpose: to check the status of the a20 line in a completely self-contained state-preserving way.
;          The function can be modified as necessary by removing push's at the beginning and their
;          respective pop's at the end if complete self-containment is not required.
;
; Returns: 0 in ax if the a20 line is disabled (memory wraps around)
;          1 in ax if the a20 line is enabled (memory does not wrap around)
 
check_a20:
    pushf
    push ds
    push es
    push di
    push si
 
    cli
 
    xor ax, ax ; ax = 0
    mov es, ax
 
    not ax ; ax = 0xFFFF
    mov ds, ax
 
    mov di, 0x0500
    mov si, 0x0510
 
    mov al, byte [es:di]
    push ax
 
    mov al, byte [ds:si]
    push ax
 
    mov byte [es:di], 0x00
    mov byte [ds:si], 0xFF
 
    cmp byte [es:di], 0xFF
 
    pop ax
    mov byte [ds:si], al
 
    pop ax
    mov byte [es:di], al
 
    mov ax, 0
    je check_a20__exit
 
    mov ax, 1
 
check_a20__exit:
    pop si
    pop di
    pop es
    pop ds
    popf
 
    ret



; Enabling A20 using BIOS method
; BIOS int 15 ref http://www.ctyme.com/intr/int-15.htm
enable_a20_bios:
    pusha
    mov     ax,0x2403                ;--- A20-Gate Support ---
    int     15h
    jb      a20_ns                  ;INT 15h is not supported
    cmp     ah,0
    jnz     a20_ns                  ;INT 15h is not supported
    
    mov     ax,0x2402               ;--- A20-Gate Status ---
    int     15h
    jb      a20_failed              ;couldn't get status
    cmp     ah,0
    jnz     a20_failed              ;couldn't get status
    
    cmp     al,1
    jz      a20_activated           ;A20 is already activated
    
    mov     ax,2401h                ;--- A20-Gate Activate ---
    int     15h
    jb      a20_failed              ;couldn't activate the gate
    cmp     ah,0
    jnz     a20_failed              ;couldn't activate the gate

a20_failed:                     ; set carry flag if failed or not supported
a20_ns:
    stc
    popa
    ret
a20_activated:                  ; clear carry flag if success
    clc
    popa
    ret


; Enable A20 address line by Keyboard controller method
; Ref: https://wiki.osdev.org/%228042%22_PS/2_Controller
enable_a20_keyboard:
    cli
    push ax

    call    a20wait
    mov     al,0xAD     ; Command: Disable keyboard (Disable first PS/2 port)
    out     0x64,al

    call    a20wait
    mov     al,0xD0     ; Command: Read Controller Output Port
    out     0x64,al

    call    a20wait2
    in      al,0x60     ; Read Controller Output Port
    push    ax

    call    a20wait
    mov     al,0xD1     ; Command: Write next byte to Controller Output Port
    out     0x64,al

    call    a20wait
    pop     ax
    or      al,2        ; Set A20 bit and
    out     0x60,al     ; Write to Controller Output Port

    call    a20wait
    mov     al,0xAE
    out     0x64,al     ; Command: Enable keyboard (Enable first PS/2 port)

    call    a20wait
    pop ax
    sti
    ret
; Using keyboard controller port to simulate a sleep/time-out wait
a20wait:
    in      al,0x64
    test    al,2 ; System Flag, set if the system passes self tests (POST)
    jnz     a20wait
    ret
; To check if output buffer is empty before writing to Controller Output Port
a20wait2:
    in      al,0x64
    test    al,1; Input buffer status (0 = empty, 1 = full)
    jz      a20wait2
    ret

; Enable A20 address line by Fast A20 method
enable_a20_fast:
    push ax
    in al, 0x92
    test al, 2
    jnz enable_a20_fast_after
    or al, 2
    and al, 0xFE
    out 0x92, al
enable_a20_fast_after:
    pop ax
    ret