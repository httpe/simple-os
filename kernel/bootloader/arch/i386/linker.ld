/* 
From: http://3zanders.co.uk/2017/10/18/writing-a-bootloader3/
*/

ENTRY(boot)
SECTIONS {
	/* Telling the linker that the bootloader will be loaded at 0x7c00, the convensional boot sector address */
    . = 0x7c00;
	/* Make sure the .boot section is at the start of the file, so the 0xAA55 magic number/MBR signature will be placed at the correct location */
    .text :
    {
        *(.boot)
        *(.text)
    }
    .rodata :
    {
        *(.rodata)
    }
    .data :
    {
        *(.data)
    }
    .bss :
    {
        *(.bss)
    }
}