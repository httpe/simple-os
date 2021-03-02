    ENTRY(call_user_main)
    SECTIONS
    {
        . = 0x1000;                 /* relocate to address 0x1000, so 0x0 is not page mapped, dangling pointer will fail the program earlier */

        .text ALIGN(4):
        {
            *(.text)           /* include all other .text sections */
        }

        .data ALIGN(4):
        {
            *(.data)
        }

        .rodata ALIGN(4):
        {
            *(.rodata*)
        }

        .bss ALIGN(4K):
        {
            *(.bss)
        }
    }