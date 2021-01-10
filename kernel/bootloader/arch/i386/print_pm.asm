; global print_pm, WHITE_ON_BLACK, print_memory_hex

[bits 32] ; using 32-bit protected mode

; this is how constants are defined
VIDEO_MEMORY equ 0xb8000
WHITE_ON_BLACK equ 0000_1111b ; color attribute: white character on black background


; print string in 32-bit protected mode using VGA text mode
; The screen will display 80 columns x 25 rows  colored characters
; input
; esi = the starting address of a null terminated string
; ah = the char color attribute
; edi = 0-based character index on screen (0 ~ 16000 = 80 columns x 25 rows  x 8 screens) 
; Ref: https://en.wikipedia.org/wiki/VGA_text_mode
; Attribute bits: [Blink BackgroundRed BackgroundGreen BackgroundBlue ForegroundIntensity ForegroundRed ForegroundGreen ForegroundBlue]
; E.g. 0000_1111b means black background (R=G=B=0 -> Black) and white foreground/font (R=G=B=1 -> White) and show in intensified font
print_pm:
    pusha
    add edi, edi                ; each characters occupy two bytes
    add edi, VIDEO_MEMORY

print_pm_loop:
    mov al, [esi]               ; [esi] is the address of our character

    cmp al, 0                   ; check if end of string
    je print_pm_done

    mov [edi], ax               ; store character + attribute in video memory
    add esi, 1                  ; next char
    add edi, 2                  ; next video memory position

    jmp print_pm_loop

print_pm_done:
    popa
    ret


; convert a byte in dl to its hex representation by two ASCII chars storing in ax
; dl = 0x1A -> dl = '1', dh = 'A'
byte_to_hex_string:
    mov dh, dl   ; dh = dl = 0x1A
    shr dl, 4 ; dl: 0x1A -> 0x01
    add dl, 0x30 ; add 0x30 to 1 to convert it to ASCII "1"
    cmp dl, 0x39 ; if > 9, add extra 8 to represent 'A' to 'F'
    jle .next_char
    add dl, 7 ; 'A' is ASCII 65 instead of 58, so 65-58=7
.next_char:
    and dh, 0x0f ; dh: 0x1A -> 0x0A
    add dh, 0x30 ; add 0x30 to A to convert it to ASCII "A"
    cmp dh, 0x39 ; if > 9, add extra 8 to represent 'A' to 'F'
    jle .byte_to_hex_string_end
    add dh, 7 ; 'A' is ASCII 65 instead of 58, so 65-58=7
.byte_to_hex_string_end:
    ret

; print a block of memory in hex
; input
; esi: print memory location start from here
; ecx: count (max 65535)
; ah: char color attribute
; edi = 0-based character index on screen (0 ~ 16000 = 80 rows x 25 columns x 8 screens), should be multiple of 80
print_memory_hex:
    pusha
    mov ebx, 0                   ; index of the current byte

    add edi, edi
    add edi, VIDEO_MEMORY

.loop:
    mov dl, [esi+ebx]
    call byte_to_hex_string

    ; print '1A ', each byte will need 3 ASCII characters
    mov al, dl
    mov [edi], ax
    mov al, dh
    mov [edi+2], ax
    mov al, ' '
    mov [edi+4], ax            

    add edi, 6
    inc ebx

    ; if the byte just printed is multiple of 16, wrap to new line

    push eax        ; save ah
    push ebx
    
    mov eax, ebx
    mov edx, 0
    mov ebx, 16
    div ebx
    
    pop ebx
    pop eax         ; restore ah

    cmp edx, 0      ; test if ebx mod 16 = 0
    jnz .next
    push ecx
    mov ecx, 32
    mov al, ' '
.padding:           ; wrap to new line by 32 space characters as padding, since 16 bytes will already occupy 16*3 = 48 characters, so 80 - 48 = 32
    mov [edi], ax
    add edi, 2
    loop .padding
    pop ecx
.next:
    loop .loop

    popa
    ret
