
; Hard coded screen settings
; Will find and switch to the VESA mode matching these criteria
; Hang if not found
SCREEN_RESOLUTION_WIDTH equ 800
SCREEN_RESOLUTION_HEIGHT equ 600
; We are assuming color depth is always 32 in other code
; Do not change this unless you believe everything is in sync
SCREEN_COLOR_DEPTH equ 32

; declare the address of VESA mode selected as global variables
; so it can be accessed in C
global VESA_BIOS_INFO, VESA_MODE_INFO, VESA_MODE, VGA_FONT_ADDR
VESA_BIOS_INFO times 512 db 0
VESA_MODE_INFO times 256 db 0
VESA_MODE dw 0
VGA_FONT_ADDR dw 0x0500 ; use conventional memory to store BIOS VGA fonts

; Switch to specified VESA video modes
; Ref: https://wiki.osdev.org/User:Omarrx024/VESA_Tutorial
; Receive `di` as start of a free space, at least 2KiB big
switch_vesa_mode:
    pusha
    push ds

    mov di, VESA_BIOS_INFO

    ; Add signature to the structure before call
    ; telling the BIOS that we support VBE 2.0+
    ; by writing "VBE2" at the start of result buffer di
    mov cx, 4
    mov si, STR_VBE2
    call memcpy

    ; Get VESA BIOS information
    ; Function code: 0x4F00
    ; Description: Returns the VESA BIOS information, including manufacturer, supported modes, available video memory, etc... Input: AX = 0x4F00
    ; Input: ES:DI = Segment:Offset pointer to where to store VESA BIOS information structure.
    ; Output: AX = 0x004F on success, other values indicate that VESA BIOS is not supported.
    ; The structure returned is defined in video/video.h as vbe_mode_info_structure
    mov ax, 0x4F00
    int 0x10

    cmp ax, 0x004F
    je .get_vesa_bios_success
    mov bx, MSG_VESA_BIOS_FAILED
    call print
    jmp $
.get_vesa_bios_success:
    mov cx, 4
    mov si, STR_VESA
    call memcmp
    je .vesa_bios_info_verified
    mov bx, MSG_VESA_BIOS_FAILED
    call print
    jmp $
.vesa_bios_info_verified:
    ; mov bx, MSG_VESA_SUCCESS
    ; call print
    ; call print_nl

    ; mov cx, 34
    ; mov bx, di
    ; call print_hex_cx
    ; call print_nl

    ; video_modes offset part
    mov bx, [di + 14]
    ; video_modes segment part
    mov dx, [di + 16]
    push dx ; new ds segment

    ; + 512 bytes to get to a free space to store VESA mode info
    ; since struct vbe_info_structure is of size 512 bytes
    ; add di, 0x200
    mov di, VESA_MODE_INFO

    ; mov cx, 256
    ; call print_hex_cx

    mov si, bx
.check_next_vesa_mode:

    pop ds  ; new ds segment

    mov dx, [si]
    add si, 2
    
    ; restore to orig segments
    pop ax  ; orig es
    push ax
    push ds ; new ds
    mov ds, ax
    
    cmp dx, 0xFFFF
    jne .get_mode_detail

    mov bx, MSG_VESA_NO_MATCHED_MODE
    call print
    jmp $

.get_mode_detail:
    mov cx, dx  ; VESA mode number

    ; Get VESA mode information
    ; Function code: 0x4F01
    ; Description: This function returns the mode information structure for a specified mode. The mode number should be gotten from the supported modes array.
    ; Input: AX = 0x4F01
    ; Input: CX = VESA mode number from the video modes array
    ; Input: ES:DI = Segment:Offset pointer of where to store the VESA Mode Information Structure shown below.
    ; Output: AX = 0x004F on success, other values indicate a BIOS error or a mode-not-supported error.
    ; The structure returned is defined in video/video.h as vbe_mode_info_structure
    mov ax, 0x4F01
    int 0x10

    cmp ax, 0x004F
    je .vesa_success1
    mov bx, MSG_VESA_BIOS_FAILED
    call print
    jmp $
.vesa_success1:
    ; get .attributes
    mov ax, [di]
    ; check if the mode support linear frame buffer
    ; skip those do not support
    and ax, 0x80
    cmp ax, 0x80
    jne .check_next_vesa_mode

    mov [VESA_MODE], dx

    ; get .bpp
    mov al, [di + 25]
    ; check if the mode support 32 bits color
    ; skip those do not support
    cmp al, SCREEN_COLOR_DEPTH
    jne .check_next_vesa_mode

    ; get .width
    mov dx, [di + 18]
    cmp dx, SCREEN_RESOLUTION_WIDTH
    jne .check_next_vesa_mode

    ; get .height
    mov dx, [di + 20]
    cmp dx, SCREEN_RESOLUTION_HEIGHT
    jne .check_next_vesa_mode

    ; mov bx, [di + 27]
    ; mov cx, 1
    ; call print_hex_cx
    ; call print_nl

    ; jmp .check_next_vesa_mode

.vesa_good_mode_found:

    ; FUNCTION: Set VBE mode
    ; Function code: 0x4F02
    ; Description: This function sets a VBE mode.
    ; Input: AX = 0x4F02
    ; Input: BX = Bits 0-13 mode number; bit 14 is the LFB bit: when set, it enables the linear framebuffer, when clear, software must use bank switching. Bit 15 is the DM bit: when set, the BIOS doesn't clear the screen. Bit 15 is usually ignored and should always be cleared.
    ; Output: AX = 0x004F on success, other values indicate errors; such as BIOS error, too little video memory, unsupported VBE mode, mode doesn't support linear frame buffer, or any other error.
    mov ax, 0x4F02	; set VBE mode
    mov bx, [VESA_MODE]
    ; VBE mode number; notice that bits 0-13 contain the mode number and bit 14 (LFB) is set and bit 15 (DM) is clear.
    or bx, 0x4000 ; set bit 14 (LFB)
    and bx, 0x7FFF ; clear bit 15 (DM)
    int 0x10			; call VBE BIOS

    cmp ax, 0x004F
    je .vesa_set_mode_success
    mov bx, MSG_VESA_BIOS_FAILED
    call print
    jmp $
    
.vesa_set_mode_success:
    ; mov bx, MSG_VESA_SELECT_SUCCESS
    ; call print

    pop ax ; VESA ds segment
    pop ds;  original ds segment

    ; Get BIOS VGA font
    ; Ref: https://wiki.osdev.org/VGA_Fonts
    mov di, [VGA_FONT_ADDR]
    ;in: es:di=4k buffer
    ;out: buffer filled with font
    push			ds
    push			es
    ;ask BIOS to return VGA bitmap fonts
    mov			ax, 1130h
    mov			bh, 6
    int			10h
    ;copy charmap
    push		es
    pop			ds
    pop			es
    mov			si, bp
    mov			cx, 256*16/4
    rep			movsd
    pop			ds


    popa
    ret

STR_VBE2 db "VBE2"
STR_VESA db "VESA"
MSG_VESA_VERSION db "VESA version: ", 0
MSG_VESA_SELECT_SUCCESS db "Select VESA mode success!", 0
MSG_VESA_BIOS_FAILED db "VESA BIOS operation failed!", 0
MSG_VESA_NO_MATCHED_MODE db "No VESA mode satisfying criteria!", 0
