/* The bootloader will look at this image and start execution at the symbol
   designated at the entry point. */
ENTRY(_start)

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	/* these labels get exported to the code files */
	MAP_MEM_PA_ZERO_TO = 0xC0000000; /* This must be in sync with boot.asm */
	KERNEL_PHYSICAL_START = 0x100000;
	KERNEL_VIRTUAL_START = MAP_MEM_PA_ZERO_TO + KERNEL_PHYSICAL_START;

	/* Begin putting sections at 1 MiB, a conventional place for kernels to be
	   loaded at by the bootloader. */
	. = KERNEL_PHYSICAL_START;

	/* First put the multiboot header, as it is required to be put very early
	   early in the image or the bootloader won't recognize the file format. */
	.multiboot : {
		*(.multiboot)
	}

	/* The kernel will live at 3GB + 1MB in the virtual address space, */
	/* which will be mapped to 1MB in the physical address space. */
	/* "." is used to determine the absolute addresses in the generated binary file 
	   they will be generated based on the offset plus the value of .
	*/
	/* Adding 0xC0000000 to pertend the whole program was loaded at 0xC0000000 
	   (use += instead of than = to takle the size of multiboot into consideration */
	. += MAP_MEM_PA_ZERO_TO;

	/* Next we'll put the .text section. */
	/* The section is aligned in 4KiB boundary */
	/* AT(addr) means to actually ask the ELF loader to load this section at addr */
	/* In combination of ". = 0xC0000000", we generated the binary with all address based on 0xC0000000 
	   but actually ask the bootloader to load the sections starting from 0x100000 (1MiB) */
	.text ALIGN (4K) : AT (ADDR (.text) - MAP_MEM_PA_ZERO_TO)
	{
		*(.text)
	}

	/* Read-only data. */
	.rodata ALIGN (4K) : AT (ADDR (.rodata) - MAP_MEM_PA_ZERO_TO)
	{
		*(.rodata)
	}

	/* Read-write data (initialized) */
	.data ALIGN (4K) : AT (ADDR (.data) - MAP_MEM_PA_ZERO_TO)
	{
		*(.data)
	}

	/* Read-write data (uninitialized) and stack */
	.bss ALIGN (4K) : AT (ADDR (.bss) - MAP_MEM_PA_ZERO_TO)
	{
		*(COMMON)
		*(.bss)
	}

	KERNEL_VIRTUAL_END = .;
	KERNEL_PHYSICAL_END = KERNEL_VIRTUAL_END - MAP_MEM_PA_ZERO_TO;

	/* The compiler may produce other sections, put them in the proper place in
	   in this file, if you'd like to include them in the final kernel. */
}
