# README

1. Setup WSL
    <https://docs.microsoft.com/en-us/windows/wsl/install-win10>
    WSL1 is recommended to avoid issue with Hyper-V (e.g. interfering Android simulators)
1. Setup GCC cross compiler
    See: <https://wiki.osdev.org/GCC_Cross-Compiler>
1. Adjust Makefile
    Notably, point `CC` to your cross-compiler, `KERNEL_ROOT` to your kernel root folder, `KERNEL_BOOT_IMG` to your kernel image

    A kernel to test booting is <https://wiki.osdev.org/Meaty_Skeleton>

    The `KERNEL_BOOT_IMG` shall be a ELF executable
1. Compile
    In WSL:

    ```bash
    make clean
    make
    ```

    A bootable binary file will be created named `bootable_kernel.bin`
1. Boot with QEMU
    QEMU needs GTK, so we will rather run the QEMU from Windows PowerShell

    ```ps
    qemu-system-i386.exe -hda .\bootable_kernel.bin
    ```

1. Debugging by GDB
    See <https://github.com/cfenollosa/os-tutorial/tree/master/14-checkpoint>
    In Windows:

    ```ps
    qemu-system-i386.exe -s -S -hda .\bootable_kernel.bin
    ```

    Ref: <https://www.qemu.org/docs/master/system/gdb.html>
    In WSL:

    ```bash
    gdb -ex "target remote localhost:1234" -ex "symbol-file bootloader.elf"
    ```
