# Simple-OS

Simple-OS is targeted to be a self-hosting operating system, i.e. you can compile itself in itself.

The CPU architecture currently supported is x86-32.

## Prerequisite

To read the code, you need to be familiar with x86 assembly language and C.

[Tutorialspoint Assembly Programming Tutorial](https://www.tutorialspoint.com/assembly_programming/index.htm) provide a good tutorial for [NASM](https://en.wikipedia.org/wiki/Netwide_Assembler) assembly, which is what we use in this project.

We also use Makefile to automate the compile process. The [Tutorialspoint Unix Makefile Tutorial](https://www.tutorialspoint.com/makefile/index.htm) is a good introduction.

A one-stop shop for OS development is the [OsDev Wiki](https://wiki.osdev.org/Main_Page). You can find various useful resources there, and there are bunch of [tutorials](https://wiki.osdev.org/Tutorials) to follow. It is highly recommended to take look at the [Bare Bones](https://wiki.osdev.org/Bare_Bones) tutorial at OsDev Wiki to get a feel on how to get started with OS development.

## Dependencies

To compile this project, you need to install the following dependencies:

1. [NASM Assembler](https://www.nasm.us/)

1. GCC & Binutils for x86: It is recommended to compile a cross-compiler [for your own](https://wiki.osdev.org/GCC_Cross-Compiler). Your system shall also have GCC tool chain installed, since we will use utility like GNU Make

1. [QEMU](https://www.qemu.org/) Emulator: We will use QEMU to emulate our system, avoiding restarting computer again and again to test it.

1. (Optional) GRUB: The `kernel/` part can use GRUB to generate a bootable ISO image (`kernel/iso.sh`), but since we have our own `bootloader/` implemented, it is not required.

## Compilation

Firstly, you can need to change the `CROSSCOMPILERBIN` variable in `kernel/config.sh` and to point to the folder containing the cross-compiling GCC/Binutils binaries (see *Dependecies* section). Also point `CC` variable in `bootloader/Makefile` to your cross-compiling GCC.

Then you can compile the project by:

```bash
cd kernel
./clean.sh
./build.sh
cd ../bootloader
make clean
make
```

If compile successfully, `kernel/bootable_kernel.bin` will be generated. You can then test it using QEMU. Here we installed QEMU in Windows, because QEMU needs GTK and WSL graphical support is limited. We can run the QEMU from Windows PowerShell:

```ps
qemu-system-i386.exe -hda .\bootable_kernel.bin
```

## Folder Structure



## Reference

Multiple tutorials have been referenced in the development of this project.

The `kernel/` folder is basically a clone from the [Meaty Skeleton](https://wiki.osdev.org/Meaty_Skeleton) tutorial, with assembly code being translated to use intel/NASM syntax. The main development later on will happen in `kernel/` folder.

Files in the `bootloader/` folder are mostly combination of pieces scattered across many topics on the OsDev Wiki.

### OS Development Tutorials

1. [OsDev WikiTutorial Series](https://wiki.osdev.org/Tutorials): Very useful starting point of OS development. You can start with [Bare Bones](https://wiki.osdev.org/Bare_Bones) and then look into [Meaty Skeleton](https://wiki.osdev.org/Meaty_Skeleton), which we actual use in this project.

1. For tutorial in Chinese, I would recommend the [INWOX project](https://github.com/qvjp/INWOX/wiki)

1. [os-tutorial](https://github.com/cfenollosa/os-tutorial): A very user friendly step by step OS dev tutorial. Many files of the bootloader or the kernel are modified from this repo. This tutorial also combine a lot of pieces from some of the following tutorials.

1. [Roll your own toy UNIX-clone OS](http://www.jamesmolloy.co.uk/tutorial_html/) a well known tutorial, very concise and easy to understand. You shall read it with the [errata](https://wiki.osdev.org/James_Molloy%27s_Tutorial_Known_Bugs) at hand.

1. [The little book about OS development](http://littleosbook.github.io/#) and [Writing a Simple Operating System - from Scratch](https://www.cs.bham.ac.uk/~exr/lectures/opsys/10_11/lectures/os-dev.pdf). They provide some useful explanations to OS concepts and have many exemplary code. It can used as a complement to the OSDev Wiki.

1. [MIT 6.S081 Operating System Engineering Course](https://pdos.csail.mit.edu/6.828/2020/schedule.html). This course provide lecture video this year, and it is based on a very popular educational system Xv6, which is a full functioning multi-process OS within less than 10K lines of code. It can be serve as our reference implementation. The course switched to RISC-V architecture in 2019 ([Xv6-RISC](https://github.com/mit-pdos/xv6-riscv)), however for our purpose, the [x86 version](https://github.com/mit-pdos/xv6-public) will be more useful. It also provide a [functional classification](https://pdos.csail.mit.edu/6.828/2018/xv6/xv6-rev10.pdf) of each file in the x86 version.

### Documentation

1. [NASM Documentation](https://www.nasm.us/xdoc/2.15.05/html/nasmdoc0.html)
1. [GNU Make Documentation](https://www.gnu.org/software/make/manual/make.html)
1. [ELF Format](http://www.skyfree.org/linux/references/ELF_Format.pdf)
1. [x86 Instuction Set Manual](https://www.felixcloutier.com/x86/)

## Current Status & Goal

Although the final goal is to make the system self-hosting, we planned for several practical milestone:

1. **Milestone One: Bootloader**
    Many of the existing tutorials rely on [GRUB](https://en.wikipedia.org/wiki/GNU_GRUB) to boot the system and do some initialize (like entering Protected Mode), which I don't like. For educational purpose, I want to have have full control from the boot to the very end. Therefore, we choose to implement a standalone bootloader as our first step.
    - Enter 32-bit protected mode and do various initialize before passing control to the kernel
    - Provide disk I/O routines and a (read-only) file system to host the kernel
    - Able to parse and load a ELF kernel written in C
    - Provide basic printing utilities for debugging
    - Code should be self-contained, i.e. no external dependency.
    - **Finished**: Implemented under the folder `bootloader`, we choose [USTAR](https://wiki.osdev.org/USTAR) as the read-only filesystem.

1. **Milestone Two: Kernel with Input and Output**
    - Write in C, compile to a Multiboot ELF kernel file
    - Initialize IDT and ISR
    - Enable Paging
    - Support `printf` to write string to the screen
    - Support reading user input from keyboard
    - User can enter text to the screen and edit/delete it
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
    - Write a game in user mode

1. **Milestone Five: User Space Compiler**
    - Port a simplified C compiler to the system
    - Compile the game inside the system

1. **Milestone Six: Networking**
    - Allow connecting to Internet
    - Implement DNS query and ping command

1. **Final Goal: Self-hosting**
    - Port a sophisticate enough compiler to the system
    - Compile the source code of the system inside the system
    - The system is now self-hosting
