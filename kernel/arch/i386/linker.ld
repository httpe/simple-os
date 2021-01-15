/* The bootloader will look at this image and start execution at the symbol
   designated at the entry point. */
ENTRY(_start)

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	/* Begin putting sections at 1 MiB, a conventional place for kernels to be
	   loaded at by the bootloader. */
	. = 0x100000;

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
	. += 0xC0000000;

	/* Next we'll put the .text section. */
	/* The section is aligned in 4KiB boundary */
	/* AT(addr) means to actually ask the ELF loader to load this section at addr */
	/* In combination of ". = 0xC0000000", we generated the binary with all address based on 0xC0000000 
	   but actually ask the bootloader to load the sections starting from 0x100000 (1MiB) */
	.text ALIGN (4K) : AT (ADDR (.text) - 0xC0000000)
	{
		*(.text)
	}

	/* Read-only data. */
	.rodata ALIGN (4K) : AT (ADDR (.rodata) - 0xC0000000)
	{
		*(.rodata)
	}

	/* Read-write data (initialized) */
	.data ALIGN (4K) : AT (ADDR (.data) - 0xC0000000)
	{
		*(.data)
	}

	/* Read-write data (uninitialized) and stack */
	.bss ALIGN (4K) : AT (ADDR (.bss) - 0xC0000000)
	{
		*(COMMON)
		*(.bss)
	}

	/* The compiler may produce other sections, put them in the proper place in
	   in this file, if you'd like to include them in the final kernel. */
}
