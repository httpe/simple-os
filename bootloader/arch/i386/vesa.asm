
; Hard coded screen settings
; Will find and switch to the VESA mode matching these criteria
; Hang if not found
SCREEN_RESOLUTION_WIDTH equ 800
SCREEN_RESOLUTION_HEIGHT equ 600
SCREEN_COLOR_DEPTH equ 32

; declare the address of VESA mode selected as global variables
; so it can be accessed in C
global VESA_MODE, VESA_WIDTH, VESA_HEIGHT, VESA_PITCH, VESA_COLOR_DEPTH, VESA_FRAME_BUFFER_LO, VESA_FRAME_BUFFER_HI, VGA_FONT_ADDR
VESA_MODE dw 0
VESA_WIDTH dw 0
VESA_HEIGHT dw 0
VESA_PITCH dw 0
VESA_COLOR_DEPTH db 0
VESA_FRAME_BUFFER_LO dw 0
VESA_FRAME_BUFFER_HI dw 0
VGA_FONT_ADDR dw 0

; Switch to specified VESA video modes
; Ref: https://wiki.osdev.org/User:Omarrx024/VESA_Tutorial
; Receive `di` as start of a free space, at least 2KiB big
switch_vesa_mode:
    pusha
    push ds

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
    ; Here's the structure returned by this function in ES:DI:
    ; struct vbe_info_structure {
    ;     char signature[4] = "VESA";	// must be "VESA" to indicate valid VBE support: 0
    ;     uint16_t version;			// VBE version; high byte is major version, low byte is minor version: 4
    ;     uint32_t oem;			// segment:offset pointer to OEM: 6
    ;     uint32_t capabilities;		// bitfield that describes card capabilities: 10
    ;     uint32_t video_modes;		// segment:offset pointer to list of supported video modes: 14
    ;     uint16_t video_memory;		// amount of video memory in 64KB blocks: 18
    ;     uint16_t software_rev;		// software revision: 20
    ;     uint32_t vendor;			// segment:offset to card vendor string: 22
    ;     uint32_t product_name;		// segment:offset to card model name: 26
    ;     uint32_t product_rev;		// segment:offset pointer to product revision: 30
    ;     char reserved[222];		// reserved for future expansion: 34
    ;     char oem_data[256];		// OEM BIOSes store their strings in this area: 256
    ; } __attribute__ ((packed));
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
    add di, 0x200

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
    ; Here's the structure returned by this function in ES:DI:
    ; struct vbe_mode_info_structure {
    ;     uint16 attributes;		// deprecated, only bit 7 should be of interest to you, and it indicates the mode supports a linear frame buffer.: 0
    ;     uint8 window_a;			// deprecated: 2
    ;     uint8 window_b;			// deprecated: 3
    ;     uint16 granularity;		// deprecated; used while calculating bank numbers: 4
    ;     uint16 window_size;       // : 6
    ;     uint16 segment_a;         // : 8
    ;     uint16 segment_b;         // : 10
    ;     uint32 win_func_ptr;		// deprecated; used to switch banks from protected mode without returning to real mode: 12
    ;     uint16 pitch;			// number of bytes per horizontal line: 16
    ;     uint16 width;			// width in pixels: 18
    ;     uint16 height;			// height in pixels: 20
    ;     uint8 w_char;			// unused...: 22
    ;     uint8 y_char;			// ...: 23
    ;     uint8 planes;         // : 24
    ;     uint8 bpp;			// bits per pixel in this mode: 25
    ;     uint8 banks;			// deprecated; total number of banks in this mode: 26
    ;     uint8 memory_model;   // : 27
    ;     uint8 bank_size;		// deprecated; size of a bank, almost always 64 KB but may be 16 KB...: 28
    ;     uint8 image_pages;    // : 29
    ;     uint8 reserved0;      // : 30
    
    ;     uint8 red_mask;       // : 31
    ;     uint8 red_position;   // : 32
    ;     uint8 green_mask;     // : 33
    ;     uint8 green_position; // : 34
    ;     uint8 blue_mask;      // : 35
    ;     uint8 blue_position;  // : 36
    ;     uint8 reserved_mask;  // : 37
    ;     uint8 reserved_position;  // : 38
    ;     uint8 direct_color_attributes;    // : 39
    
    ;     uint32 framebuffer;		// physical address of the linear frame buffer; write here to draw to the screen: 40
    ;     uint32 off_screen_mem_off;    // : 44
    ;     uint16 off_screen_mem_size;	// size of memory in the framebuffer but not being displayed on the screen : 48
    ;     uint8 reserved1[206]; // : 50
    ; } __attribute__ ((packed));
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
    mov [VESA_COLOR_DEPTH], al

    ; get .width
    mov dx, [di + 18]
    cmp dx, SCREEN_RESOLUTION_WIDTH
    jne .check_next_vesa_mode
    mov [VESA_WIDTH], dx

    ; get .height
    mov dx, [di + 20]
    cmp dx, SCREEN_RESOLUTION_HEIGHT
    jne .check_next_vesa_mode
    mov [VESA_HEIGHT], dx    

    mov dx, [di + 16]
    mov [VESA_PITCH], dx

    mov dx, [di + 40]
    mov [VESA_FRAME_BUFFER_LO], dx
    mov dx, [di + 42]
    mov [VESA_FRAME_BUFFER_HI], dx

    ; mov bx, VESA_MODE
    ; mov cx, 13
    ; call print_hex_cx
    ; call print_nl

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
    add di, 0x200
    mov [VGA_FONT_ADDR], di
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
