# Simple-OS

Simple-OS is targeted to be a self-hosting operating system, i.e. you can compile itself in itself.

The currently supported CPU architecture is x86-32.

## Prerequisite

To read the code, you need to be familiar with x86 assembly language and C.

[Tutorialspoint Assembly Programming Tutorial](https://www.tutorialspoint.com/assembly_programming/index.htm) provide a good tutorial for [NASM](https://en.wikipedia.org/wiki/Netwide_Assembler) assembly, which is the dialect we use in this project.

We also use Makefile to automate the compile process. The [Tutorialspoint Unix Makefile Tutorial](https://www.tutorialspoint.com/makefile/index.htm) is a good introduction.

A one-stop shop for OS development is the [OsDev Wiki](https://wiki.osdev.org/Main_Page). You can find various useful resources there, and there are bunch of [tutorials](https://wiki.osdev.org/Tutorials) to follow. It is highly recommended to take a ook at the [Bare Bones](https://wiki.osdev.org/Bare_Bones) tutorial at OsDev Wiki to get a feel on how to get started with OS development.

## Dependencies

To compile this project, you need to install the following dependencies:

1. [NASM Assembler](https://www.nasm.us/)

1. GCC & Binutils for x86: It is recommended to compile a cross-compiler [for your own](https://wiki.osdev.org/GCC_Cross-Compiler). Your system shall also have GCC tool chain installed, since we will use utility like GNU Make

1. [QEMU](https://www.qemu.org/) Emulator: We will use QEMU to emulate our system, avoiding restarting computer again and again just to test the system.

1. (Optional) [GRUB](https://www.gnu.org/software/grub/): The `iso.sh` uses GRUB to generate a bootable ISO image, but since we have our own `bootloader/` implemented, it is not required.

## Build

### Configure

Firstly, you need to change the `CROSSCOMPILERBIN` variable in `config.sh` to point it to the folder containing the cross-compiling GCC/Binutils binaries (see *Dependecies* section). Note that the env variable `AS` is assumed to be the system wide NASM assembler, if not set, `nasm` is used.

Here we assume a Windows + [WSL](https://docs.microsoft.com/en-us/windows/wsl/install-win10) environment. We install QEMU in Windows, because QEMU needs GTK and WSL graphical support is limited.

### Compile

Then you can **build** the project by running in WSL:

```bash
./clean.sh
./build.sh
```

If compile finish successfully, `bootable_kernel.bin` will be generated.

### Emulate

You can then test it using QEMU.

We can run the compiled kernel in QEMU by running in Windows PowerShell:

```ps
qemu-system-i386.exe -hda bootable_kernel.bin
```

### Debug

It is possible to debug the kernel by GDB. See [QEMU GDB Usage](https://www.qemu.org/docs/master/system/gdb.html).

Start QEMU in Windows PowerShell by:

```ps
qemu-system-i386.exe -s -S -hda bootable_kernel.bin
```

And then attach GDB to the QEMU instance in WSL by:

```bash
gdb -ex "target remote localhost:1234" -ex "symbol-file sysroot/boot/myos.kernel"
```

The above loads debug symbols for the kernel, to debug our own bootloader:

```bash
gdb -ex "target remote localhost:1234" -ex "symbol-file bootloader.elf" 
```

The `bootloader.elf` is generated solely to provide the debug symbols.

This trick is also describe in the [os-tutorial](https://github.com/cfenollosa/os-tutorial/tree/master/14-checkpoint) for macOS.

### (Optional) Generate Bootable ISO

If you have GRUB installed, you can build a bootable CD-ROM image of the operating system by invoking:

```bash
./iso.sh
```

In this case, GRUB is used instead of our home made bootloader to boot the system.

To emulate it, run in Powershell:

```ps
qemu-system-i386.exe -cdrom .\myos.iso
```

### (Optional) Install Header Only

You can install all the system headers into the system root without relying on the compiler at all, which will be useful later on when switching to a Hosted GCC Cross-Compiler, by invoking:

```bash
./headers.sh
```

## Folder Structure

### Bootloader

The `bootloader/` folder contains a standalone bootloader that can load any [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) format kernel binary into memory and kick it started.

Files in the `bootloader/` folder are mostly a combination of pieces scattered across many topics on the OsDev Wiki.

The entry point is at the `boot` label in `bootloader/arch/i386/bootloader.asm`.

The `bootloader/arch/i386/` folder contains various initialization subroutines written in assembly.

- Entering 32-bit [Protected Mode](https://en.wikipedia.org/wiki/Protected_mode):
  - `switch_pm.asm`: [Switch to Protected Mode](https://github.com/cfenollosa/os-tutorial/tree/master/10-32bit-enter).
  - `a20.asm`: Enabling [A20 address line](https://wiki.osdev.org/A20).
  - `gdt.asm`: Initialize the [GDT](https://github.com/cfenollosa/os-tutorial/tree/master/09-32bit-gdt).
- Disk I/O:
  - `disk.asm`: [Read data from disk using BIOS interrupt](https://github.com/cfenollosa/os-tutorial/tree/master/07-bootsector-disk) (works in [Real Mode](https://wiki.osdev.org/Real_Mode)).
- Printing:
  - `print.asm`: [Print using BIOS interrupt](https://github.com/cfenollosa/os-tutorial/tree/master/02-bootsector-print).
  - `print_hex.asm`: [Print data as hex numbers](https://github.com/cfenollosa/os-tutorial/tree/master/05-bootsector-functions-strings).
  - `print_pm.asm`: [Print in protected mode](https://github.com/cfenollosa/os-tutorial/tree/master/08-32bit-print), using VGA video buffer.
- `linker.ld`: [Linker script](https://ftp.gnu.org/old-gnu/Manuals/ld-2.9.1/html_chapter/ld_3.html#SEC6)

Once we finish switching to protected mode, we start to write the remaining part in C.

- `bootloader/main.c`: The main C entry point, load kernel and pass control to it.
- `elf/elf.*`: Parse ELF format binary and loader it to memory.
- `tar/tar.*`: Provide basic utility to read a [USTAR](https://wiki.osdev.org/USTAR) "file system".
- `arch/i386/port_io.h`: Provide port I/O using [C inline assembly](https://wiki.osdev.org/Inline_Assembly/Examples).
- `arch/i386/ata.*`: Read data from disk by [ATA PIO mode](https://wiki.osdev.org/ATA_PIO_Mode) (works in Protected Mode), referenced [this tutorial](http://learnitonweb.com/2020/05/22/12-developing-an-operating-system-tutorial-episode-6-ata-pio-driver-osdev/)
- Printing utilities:
  - `arch/i386/tty.*`, `arch/i386/vga.h`: Write string to screen using [VGA Text Mode](https://wiki.osdev.org/Printing_To_Screen)
  - `string/string.*`: String processing utilities

### Kernel & Libc

The `kernel/` and `libc/` folders are build upon a clone from the [Meaty Skeleton](https://wiki.osdev.org/Meaty_Skeleton) tutorial.

The kernel entry point is `kernel/kernel/kernel.c`.

- `kernel/arch/i386/`
  - `boot.asm`: Intel/NASM syntax version of Meaty skeleton's boot.S
  - `gdt.asm`: Same as bootloader's flat mode GDT
  - Interrupt related
    - `idt.*`: [IDT](https://wiki.osdev.org/IDT) initialization and interrupt gate installation
    - `isr.*` and `interrupt.asm`: [CPU exceptions](https://wiki.osdev.org/Exceptions) and IRQs handlers ([ISRs](https://wiki.osdev.org/Interrupt_Service_Routines))
    - `pic.*`: [PIC](https://wiki.osdev.org/PIC) utility for IRQs
  - `port_io.h`: Inline assembly for port I/O, e.g. inb/outb ([Examples](https://wiki.osdev.org/Inline_Assembly/Examples))
  - `keyboard.*`: Keyboard driver
  - `timer.*`: [PIT](https://wiki.osdev.org/PIT) timer
  - `tty.*` and `vga.h`: VGA Text Mode driver
  - `arch_init.c`: Collection of architecture specific initializations
- `libc/`: Progressive implementation of the standard C library

## Reference

Multiple tutorials have been referenced in the development of this project.

### OS Development Tutorials

1. [OsDev WikiTutorial Series](https://wiki.osdev.org/Tutorials): Very useful starting point of learning OS development. You can start with [Bare Bones](https://wiki.osdev.org/Bare_Bones) and then look into [Meaty Skeleton](https://wiki.osdev.org/Meaty_Skeleton), which we actual use in this project.

1. For tutorial in **Chinese**, I would recommend the [INWOX project](https://github.com/qvjp/INWOX/wiki).

1. [os-tutorial](https://github.com/cfenollosa/os-tutorial): A very user friendly step by step OS dev tutorial. Many files of the bootloader or the kernel are modified from this repo. This tutorial also combines a lot of pieces from some of the following tutorials. In addition, this tutorial assumes a **macOS** environment.

1. [Roll your own toy UNIX-clone OS](http://www.jamesmolloy.co.uk/tutorial_html/) a well known tutorial, very concise and easy to understand. You shall read it with the [errata](https://wiki.osdev.org/James_Molloy%27s_Tutorial_Known_Bugs) at hand.

1. [The little book about OS development](http://littleosbook.github.io/#) and [Writing a Simple Operating System - from Scratch](https://www.cs.bham.ac.uk/~exr/lectures/opsys/10_11/lectures/os-dev.pdf). They provide explanation to exemplary code for many OS concepts. They can be used as a complement to the OSDev Wiki.

1. [MIT 6.S081 Operating System Engineering Course](https://pdos.csail.mit.edu/6.828/2020/schedule.html). This course provides lecture video this year so it can be seen as a good OS MOOC. It is based on a very popular educational system Xv6, which is a full functioning multi-process OS implemented within 10K lines of code. It will serve as our reference implementation. The course switched to RISC-V architecture in 2019 ([Xv6-RISC](https://github.com/mit-pdos/xv6-riscv)), however for our purpose, the [x86 version](https://github.com/mit-pdos/xv6-public) will be more useful. It also provides a [functional classification](https://pdos.csail.mit.edu/6.828/2018/xv6/xv6-rev10.pdf) for each file in the x86 version repo.

### Documentation

1. [NASM Documentation](https://www.nasm.us/xdoc/2.15.05/html/nasmdoc0.html)
1. [GNU Make Documentation](https://www.gnu.org/software/make/manual/make.html)
1. [ELF Format](http://www.skyfree.org/linux/references/ELF_Format.pdf)
1. [x86 Instuction Set Manual](https://www.felixcloutier.com/x86/)
1. [GCC Inline Assembly](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html)

## Current Status & Goal

Although the final goal is to make the system self-hosting, we planned for several practical milestone:

1. **Milestone One: Bootloader**
    Many of the existing tutorials rely on GRUB to boot the system and do some initialize (like entering Protected Mode), which we don't like. For educational purpose, we want to have full control over everthing, starting from booting the system. Therefore, we choose to implement a standalone bootloader as our first step.
    - Enter 32-bit protected mode and do various initialize before passing control to the kernel
    - Provide disk I/O routines and a (read-only) file system to host the kernel
    - Able to parse and load a ELF kernel written in C
    - Provide basic printing utilities for debugging
    - Code should be self-contained, i.e. no external dependency.
    - **Finished**: Implemented under the folder `bootloader`

1. **Milestone Two: Kernel with Input and Output**
    - Write in C, compile to a [Multiboot](https://wiki.osdev.org/Multiboot) ELF kernel file
    - Initialize [IDT](https://wiki.osdev.org/IDT) and [ISR](https://wiki.osdev.org/ISR)
    - Enable [Paging](https://wiki.osdev.org/Paging)
    - Support `printf` to write various type of data as string to the screen
    - Support reading user input from keyboard
    - After initialization, run a program that user can enter some text to the screen and edit/delete it
    - **In Progress**

1. **Milestone Three: Memory Management, Filesystem and Shell**
    - Implement standard C library memory allocation functions
    - Provide a readable & writable file system (FAT/Ext) and corresponding system calls/functions
    - Write a shell to allow navigating through the file system
    - Write a editor to show file content, edit and save it

1. **Milestone Four: User Space and Multiprocessing**
    - Enrich the system standard C library
    - Build our OS specific compiling tool chain
    - Allow running ELF program under user/ring3 privilege
    - Enable multi-processing
    - Write a game in user mode (e.g Tetris)

1. **Milestone Five: User Space Compiler**
    - Port a simplified C compiler to the system
    - Compile and run the game inside the system

1. **Milestone Six: Networking**
    - Allow connecting to the Internet
    - Implement DNS query and ping command

1. **Final Goal: Self-hosting**
    - Port a sophisticate enough compiler to the system
    - Compile the source code of the system inside the system
    - The system is now self-hosting
