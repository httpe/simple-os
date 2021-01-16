; Declare constants for the multiboot header.
MBALIGN  equ  1 << 0            ; align loaded modules on page boundaries
MEMINFO  equ  1 << 1            ; provide memory map
FLAGS    equ  MBALIGN | MEMINFO ; this is the Multiboot 'flag' field
MAGIC    equ  0x1BADB002        ; 'magic number' lets bootloader find the header
CHECKSUM equ -(MAGIC + FLAGS)   ; checksum of above, to prove we are multiboot

; Declare a multiboot header that marks the program as a kernel. These are magic
; values that are documented in the multiboot standard. The bootloader will
; search for this signature in the first 8 KiB of the kernel file, aligned at a
; 32-bit boundary. The signature is in its own section so the header can be
; forced to be within the first 8 KiB of the kernel file.
section .multiboot
align 4
	dd MAGIC
	dd FLAGS
	dd CHECKSUM

; The multiboot standard does not define the value of the stack pointer register
; (esp) and it is up to the kernel to provide a stack. This allocates room for a
; small stack by creating a symbol at the bottom of it, then allocating 16384
; bytes for it, and finally creating a symbol at the top. The stack grows
; downwards on x86. The stack is in its own section so it can be marked nobits,
; which means the kernel file is smaller because it does not contain an
; uninitialized stack. The stack on x86 must be 16-byte aligned according to the
; System V ABI standard and de-facto extensions. The compiler will assume the
; stack is properly aligned and failure to align the stack will result in
; undefined behavior.
section .bss align=4096
align 16
stack_bottom:
resb 524288 ; 512 KiB
stack_top:

; Preallocate pages used for paging. Don't hard-code addresses and assume they
; are available, as the bootloader might have loaded its multiboot structures or
; modules there. This lets the bootloader know it must avoid the addresses.
alignb 4096
boot_page_directory:
resb 4096
; boot_page_table1:
boot_page_table:
resb 4096
; Further page tables may be required if the kernel grows beyond 3 MiB.

; The linker script specifies _start as the entry point to the kernel and the
; bootloader will jump to this position once the kernel has been loaded. It
; doesn't make sense to return from this function as the bootloader is gone.
; Declare _start as a function symbol with the given symbol size.
section .text
global _start:function (_start_end - _start)
_start:
	; The bootloader has loaded us into 32-bit protected mode on a x86
	; machine. Interrupts are disabled. Paging is disabled. The processor
	; state is as defined in the multiboot standard. The kernel has full
	; control of the CPU. The kernel can only make use of hardware features
	; and any code it provides as part of itself. There's no printf
	; function, unless the kernel provides its own <stdio.h> header and a
	; printf implementation. There are no security restrictions, no
	; safeguards, no debugging mechanisms, only what the kernel provides
	; itself. It has absolute and complete power over the
	; machine.

	; Build the boot_page_table, mapping a block of virtual address space to the first 4MiB in physical memory
	; The actual starting virtual address of the block is determined by which page directory entry is pointing to this page table
	; We will use this fact to point two page directory entry to this same boot_page_table table
	; To achieve that:
	; 1. Map first 4MB virtual address to the same physical address
	; 2. Map the same 4MB physical address space from virtual address space starting at 0xC0000000, so the kernel code will start at 0xC0100000 (1MiB into that space)
	; The 1. ensure the CPU can still find the next instruction in memory once we enable paging
	; And 2. is the start of a Higher Half Kernel, see https://wiki.osdev.org/Higher_Half_x86_Bare_Bones
	; Basically to allow later user space program to load at virtual address 0x0 while not overriding the kernel code

	mov esi, 0			; physical address to map
	mov edi, 0			; offset into the page table
	mov ecx, 1024		; there are 1024 page entries in a page table

.loop:
	; A page table entry if masking out the lower 12 bits to zero should give the physical address of the 4KiB memory block (page) mapped
	; Since our physical address to be mapped are multiple of 4096 bytes, the lower 12 bits are already zero
	mov eax, esi
	; Attributes: supervisor level (bit 2: user=0), read/write (bit 1: rw=1), present (bit 0: p=1)
	; See https://wiki.osdev.org/Paging
	add eax, 0000_0011b

	; The "- 0xC0000000" part is because in the linker script, we assume the whole kernel will be loaded at 0xC0000000
	;   meaning all absolute address is based on 0xC0000000, while in reality the bootloader will load this assembly at 1MiB (0x100000)
	; So we need to cancel out the base address set by linker for any label in order to get the physical address
	; We need to keep doing this before we actually have paging enabled 
	mov [(boot_page_table - 0xC0000000) + edi], eax
	
	add esi, 4096		; page size is 4KiB
	add edi, 4			; page table entry size is 32 bits (4 bytes)
	loop .loop

	; set up the identity mapping entry in page directory
	; page directory entry if masking out the lower 12 bits to zero should give the physical address of the page table pointed to
	; attributes: supervisor level (bit 2: user=0), read/write (bit 1: rw=1), present (bit 0: p=1)
	mov eax, (boot_page_table - 0xC0000000)
	add eax, 3
	mov [(boot_page_directory - 0xC0000000)], eax

	; set up higher half kernel mapping in page directory
	; 0xC0000000 should be the (0xC0000000 >> 22)th entry in the page directory
	; Since the virtual address is 32[10bits page directory index; 10bits page table index; 12bits memory address offset]0
	; See https://en.wikipedia.org/wiki/Protected_mode#/media/File:080810-protected-386-paging.svg
	; And that entry should be (0xC0000000 >> 22)*4 = 768*4 = 3072 bytes into the page directory, since page directory entry is 4 bytes long
	mov [(boot_page_directory - 0xC0000000)  + (0xC0000000 >> 22)*4], eax

	; Point the last page directory entry to the page directory itself
	; This is so called Recursive Page Directory (https://blog.inlow.online/2019/02/25/Memory-Management/)
	; English Ref: http://www.rohitab.com/discuss/topic/31139-tutorial-paging-memory-mapping-with-a-recursive-page-directory/
	; In this way, accessing last 4MiB virtual memory is accessing the page tables or the page directory itself
	; e.g. read 4 bytes from virtual address [10 bits of 1;10bits of 1;0, 4, 8 etc.]
	;      you can get back the last page directory entry, which contains the physical address of the page tables
	;      and the last page directory entry contains the physical address of the directory itself
	mov eax, (boot_page_directory - 0xC0000000)
	add eax, 3
	mov [(boot_page_directory - 0xC0000000) + 1023*4], eax

	; Start enabling paging
	; Set cr3 to the address of the boot_page_directory.
	mov eax, (boot_page_directory - 0xC0000000)
	mov cr3, eax
	
	mov eax, cr0
	; Set Paging (bit 31) and Protection Mode (bit 0) bits
	; See https://en.wikipedia.org/wiki/Control_register#CR0
	or eax, 0x80000001
	mov cr0, eax

	; Jump to higher half with an absolute jump. 
    lea eax, [higher_half] ; load the address of the label in ebx
    jmp eax                ; jump to the label

higher_half:
	; code here executes in the higher half kernel
	; eip is larger than 0xC0000000
	; can continue kernel initialization, calling C code, etc.

	; Unmap the identity mapping as it is now unnecessary. 
	mov dword [boot_page_directory], 0

	; Reload CR3 to force a TLB flush so the changes to take effect.
	mov eax, cr3
	mov cr3, eax

	; To set up a stack, we set the esp register to point to the top of our
	; stack (as it grows downwards on x86 systems). This is necessarily done
	; in assembly as languages such as C cannot function without a stack.
	mov esp, stack_top
	
	; Save ebx passed by the bootloader, pointing to the multiboot_info structure
	push ebx

	; This is a good place to initialize crucial processor state before the
	; high-level kernel is entered. It's best to minimize the early
	; environment where crucial features are offline. Note that the
	; processor is not fully initialized yet: Features such as floating
	; point instructions and instruction set extensions are not initialized
	; yet. The GDT should be loaded here. Paging should be enabled here.
	; C++ features such as global constructors and exceptions will require
	; runtime support to work as well.
	
	; Start Loading GDT (Needed for GRUB, no need for our own bootloader)
    lgdt [gdt_descriptor]   ; 1. load the GDT descriptor
    jmp CODE_SEG:init_pm    ; 2. Far jump by using a different segment, basically setting CS to point to the correct protected mode segment (GDT entry)

init_pm:
    mov ax, DATA_SEG        ; 3. update the segment registers other than CS
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

	extern _init			; 4. https://wiki.osdev.org/Calling_Global_Constructors
    call _init
 
	; Enter the high-level kernel. The ABI requires the stack is 16-byte
	; aligned at the time of the call instruction (which afterwards pushes
	; the return pointer of size 4 bytes). The stack was originally 16-byte
	; aligned above and we've since pushed a multiple of 16 bytes to the
	; stack since (pushed 0 bytes so far) and the alignment is thus
	; preserved and the call is well defined.
        ; note, that if you are building on Windows, C functions may have "_" prefix in assembly: _kernel_main

	extern kernel_main
	call kernel_main
 
	; If the system has nothing more to do, put the computer into an
	; infinite loop. To do that:
	; 1) Disable interrupts with cli (clear interrupt enable in eflags).
	;    They are already disabled by the bootloader, so this is not needed.
	;    Mind that you might later enable interrupts and return from
	;    kernel_main (which is sort of nonsensical to do).
	; 2) Wait for the next interrupt to arrive with hlt (halt instruction).
	;    Since they are disabled, this will lock up the computer.
	; 3) Jump to the hlt instruction if it ever wakes up due to a
	;    non-maskable interrupt occurring or due to system management mode.
	cli
.hang:	hlt
	jmp .hang
_start_end:

%include "gdt.asm"