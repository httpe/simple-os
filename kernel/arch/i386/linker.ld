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

	/* .start_init will be executed right after entering the user space
		Below we will tell the bootloader (through EFI headers) to place code of .start_init
		at the physical memory address START_INIT_PHY_BEGIN by the "AT" instruction */
	START_INIT_PHY_BEGIN = .;
	/* VIRTUAL: After the higher half kernel memory mapping, the kernel can read start_init code at this virtual address */
	START_INIT_VIRTUAL_BEGIN = START_INIT_PHY_BEGIN + MAP_MEM_PA_ZERO_TO;
	/* RELOC: virtual address where the start_init code shall be copied to before executing
		Instruct the compiler to generate the start_init code as if it will be loaded at START_INIT_RELOC_BEGIN
		i.e. the page (0x1000 = size of one page) right below the kernel space (area >= MAP_MEM_PA_ZERO_TO) 
	*/
	START_INIT_RELOC_BEGIN = MAP_MEM_PA_ZERO_TO - 0x1000;
	. = START_INIT_RELOC_BEGIN;
	.start_init ALIGN (4K) : AT (START_INIT_PHY_BEGIN) {
		*(.start_init)
	}
	STRAT_INIT_RELOC_END = .;
	START_INIT_SIZE = STRAT_INIT_RELOC_END - START_INIT_RELOC_BEGIN;
	. = START_INIT_PHY_BEGIN + START_INIT_SIZE;

	/* The kernel will live at 3GB + 1MB in the virtual address space, */
	/* which will be mapped to 1MB in the physical address space. */
	/* "." is used to determine the absolute addresses in the generated binary file 
	   they will be generated based on the offset plus the value of .
	*/
	/* Adding 0xC0000000 to pretend the whole program was loaded at 0xC0000000 
	   (use += instead of than = to take the size of multiboot into consideration */
	. += MAP_MEM_PA_ZERO_TO;

	/* Next we'll put the .text section. */
	/* The section is aligned in 4KiB boundary */
	/* AT(addr) means to actually ask the ELF loader to load this section at addr */
	/* In combination of ". += 0xC0000000", we generated the binary with all address based on 0xC0000000 
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
