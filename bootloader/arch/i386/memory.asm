; source https://wiki.osdev.org/Detecting_Memory_(x86)#Getting_an_E820_Memory_Map
; use the INT 0x15, eax= 0xE820 BIOS function to get a memory map
; note: initially di is 0, be sure to set it to a value so that the BIOS code will not be overwritten. 
;       The consequence of overwriting the BIOS code will lead to problems like getting stuck in `int 0x15`
; outputs: global MMAP, MMAP_COUNT
;           MMAP_COUNT will store a dword count of entries;
;           MMAP the memory layout structure 
; If failed, hang

; Maximum number of memory map items
MMAP_MAX_COUNT equ 100

; E820 memory map item is 24 bytes long
MMAP_ITEM_SIZE equ 24

; declare the address of storing the memory map as global variables
; so it can be accessed in C
global MMAP, MMAP_COUNT
MMAP_COUNT dw 0
MMAP: times (MMAP_MAX_COUNT*MMAP_ITEM_SIZE) db 0

detect_memory_map_by_e820:
	pusha

    mov di, MMAP          ; Set di to 0x0504. Otherwise this code will get stuck in `int 0x15` after some entries are fetched 
	xor ebx, ebx		; ebx must be 0 to start
	xor bp, bp		; keep an entry count in bp
	mov edx, 0x0534D4150	; Place "SMAP" into edx
	mov eax, 0xe820

    mov dword [es:di], MMAP_ITEM_SIZE; populate `size` field of standard multi-boot memory layout entry
    add di, 4

    mov [es:di + 20], dword 1	; force a valid ACPI 3.X entry
	mov ecx, MMAP_ITEM_SIZE		; ask for 24 bytes
	int 0x15
	jc short .failed	; carry set on first call means "unsupported function"
	mov edx, 0x0534D4150	; Some BIOSes apparently trash this register?
	cmp eax, edx		; on success, eax must have been reset to "SMAP"
	jne short .failed
	test ebx, ebx		; ebx = 0 implies list is only 1 entry long (worthless)
	je short .failed
	jmp short .jmpin
.e820lp:
	mov eax, 0xe820		; eax, ecx get trashed on every int 0x15 call

    mov dword [es:di], MMAP_ITEM_SIZE; populate `size` field of standard multi-boot memory layout entry
    add di, 4

	mov [es:di + MMAP_ITEM_SIZE - 4], dword 1	; force a valid ACPI 3.X entry
	mov ecx, MMAP_ITEM_SIZE		; ask for 24 bytes again
	int 0x15
	jc short .e820f		; carry set means "end of list already reached"
	mov edx, 0x0534D4150	; repair potentially trashed register
.jmpin:
	jcxz .skipent		; skip any 0 length entries
	cmp cl, MMAP_ITEM_SIZE-4		; got a 24 byte ACPI 3.X response?
	jbe short .notext
	test byte [es:di + MMAP_ITEM_SIZE-4], 1	; if so: is the "ignore this data" bit clear?
	je short .skipent
.notext:
	mov ecx, [es:di + 8]	; get lower uint32_t of memory region length
	or ecx, [es:di + 12]	; "or" it with upper uint32_t to test for zero
	jz .skipent		; if length uint64_t is 0, skip entry
	inc bp			; got a good entry: ++count, move to next storage spot
	cmp bp, MMAP_MAX_COUNT
	jg .failed_too_many
	add di, MMAP_ITEM_SIZE
.skipent:
	test ebx, ebx		; if ebx resets to 0, list is complete
	jne short .e820lp
.e820f:
    ; mov ebx, [ADDR_MMAP_COUNT]
	; mov [ebx], bp	; store the entry count
	mov [MMAP_COUNT], bp
	clc			; there is "jc" on end of list to this point, so the carry must be cleared

	; Print success message
    mov bx, MSG_E820_SUCESS0
    call print
    mov dx, bp
    call print_hex
    mov bx, MSG_E820_SUCESS1
    call print
    call print_nl

	popa
	ret
.failed:
    mov bx, MSG_E820_FAILED
    call print
    jmp $
.failed_too_many:
    mov bx, MSG_E820_FAILED_TOO_MANY_SEG
    call print
    jmp $


MSG_E820_FAILED db "E820 memory detection failed, hanged", 0
MSG_E820_FAILED_TOO_MANY_SEG db "E820 detected too many segments, hanged", 0
MSG_E820_SUCESS0 db "E820 memory detection success with ", 0
MSG_E820_SUCESS1 db " memory segments", 0