# Simple-OS

Simple-OS is a (barely working) hobbyist operating system. My target is to create a self-hosting operating system, i.e. you can compile itself in itself.

Currently the only CPU architecture supported is x86-32.

## Current Status & Goal

Although the final goal is to make the system self-hosting, we have planned for several practical milestone:

1. **Milestone One: Bootloader**
    Many of the existing tutorials rely on GRUB to boot the system and do some initialize (like entering Protected Mode), which we don't like. For educational purpose, we want to fully control everything, starting from booting the system. Therefore, we choose to implement a standalone bootloader as our first step.
    - Enter 32-bit protected mode and do various initialize before passing control to the kernel
    - Provide disk I/O routines and a (read-only) file system to host the kernel
    - Able to parse and load a ELF kernel written in C
    - Provide basic printing utilities for debugging
    - Code should be self-contained, i.e. no external dependency.
    - **Finished**: Implemented under the folder `bootloader`

1. **Milestone Two: Kernel with Input and Output**
    - Write in C, compile to a [Multiboot](https://wiki.osdev.org/Multiboot) ELF kernel file
    - Initialize [IDT](https://wiki.osdev.org/IDT) and [ISR](https://wiki.osdev.org/ISR)
    - Enable [Paging](https://wiki.osdev.org/Paging) and [Higher Half Kernel](https://wiki.osdev.org/Higher_Half_x86_Bare_Bones)
    - Support `printf` to write various type of data as string to the screen
    - Support reading user input from keyboard
    - After initialization, run a program that user can enter some text to the screen and edit/delete it
    - **Finished**: Implemented under the folder `kernel`

1. **Milestone Three: Memory Management, User Space and Multiprocessing**
    - Implement a [heap](https://en.wikipedia.org/wiki/Memory_management#HEAP) to do [dynamic memory allocation](https://en.wikipedia.org/wiki/C_dynamic_memory_allocation)
    - Allow switch to user space and run ELF program under user/ring3 privilege
    - Enable multi-processing with independent kernel stack and page directory, starting with [Cooperative Scheduling](https://littleosbook.github.io/#cooperative-scheduling-with-yielding) and then [Preemptive Scheduling](https://littleosbook.github.io/#preemptive-scheduling-with-interrupts)
    - Implement `fork`, `exec` and `yield` system calls on a simple read-only file system (potentially reuse the USTAR code from the bootloader)
    - **Finished**:
      - Kernel heap
      - ELF loader
      - Cooperative and preemptive multi-tasking (single CPU, synchronized by disabling interrupt when doing system call)
      - Exec system call based on the USTAR read-only file system

1. **Milestone Four: Filesystem,  Libc and Hosted Tool-chain**
    - Provide a readable & writable file system (FAT or Ext) and corresponding system calls
    - Build user space standard C library with system calls, e.g. printf, malloc and open/read/write. Potentially port [Newlib](https://wiki.osdev.org/Porting_Newlib) as our standard C library.
    - Build our [OS specific/hosted compiling tool-chain](https://wiki.osdev.org/OS_Specific_Toolchain) (Binutils and GCC)
    - **Finished**:
      - [Simple-FS](https://github.com/httpe/simple-fs): A fully functional FAT32 file system
      - [Simple-newlib](https://github.com/httpe/simple-newlib): Ported Newlib 4.1.0 with all system calls implemented
      - [Simple-gcc](https://github.com/httpe/simple-gcc) and [Simple-binutils](https://github.com/httpe/simple-binutils): Hosted Binutils 2.35.1 / GCC 10.2.0 tool-chain established (See HostedToolchain.md)

1. **Milestone Five: Shell, User Space Applications and Compiler**
    - Write a shell to allow navigating through the file system and execute applications
    - Write an editor to show file content, allowing editing and saving to disk
    - Port a simplified C compiler to the system
    - Compile and run the text editor inside the system
    - **In Progress**
      - A shell with `cd`, `ls` and program execution was implemented
      - Ported the simple text editor [Kilo](https://viewsourcecode.org/snaptoken/kilo/index.html), with basic editing functionalities and C syntax highlighting

1. **Milestone Six: Multi-core CPU support and IPC**
    - Support multi-core CPU through [APCI](https://wiki.osdev.org/APIC)
    - Provide [synchronization mechanism](https://wiki.osdev.org/Synchronization_Primitives) for multi-tasking environment, like locks
    - Provide inter process communication ([IPC](https://wiki.osdev.org/Category:IPC)) mechanism like pipe and signal

1. **Milestone Seven: Networking**
    - Allow connecting to the Internet
    - Implement DNS query and ping command

1. **Final Goal: Self-hosting**
    - Port a sophisticate enough compiler to the system
    - Compile the source code of the system inside the system
    - The system is now self-hosting

## Prerequisite

To read the code, you need to be familiar with x86 assembly language and C.

[Tutorialspoint Assembly Programming Tutorial](https://www.tutorialspoint.com/assembly_programming/index.htm) provide a good tutorial for [NASM](https://en.wikipedia.org/wiki/Netwide_Assembler) assembly, which is the dialect we use in this project.

We also use Makefile to automate the compile process. The [Tutorialspoint Unix Makefile Tutorial](https://www.tutorialspoint.com/makefile/index.htm) is a good introduction.

A one-stop shop for OS development is the [OsDev Wiki](https://wiki.osdev.org/Main_Page). You can find various useful resources there, and there are bunch of [tutorials](https://wiki.osdev.org/Tutorials) to follow. It is highly recommended to take a look at the [Bare Bones](https://wiki.osdev.org/Bare_Bones) tutorial at OsDev Wiki to get a feel on how to get started with OS development.

## Dependencies

Our testing environment is Ubuntu 20.04 LTS, if you use the same, you can install all the mandatory dependencies (1. through 4.) by running `build-toolchain.sh`.

1. [NASM Assembler](https://www.nasm.us/).

2. [QEMU](https://www.qemu.org/) Emulator: We will use QEMU to emulate our system, avoiding restarting computer again and again just to test the system.

3. GCC & Binutils for x86: It is recommended to [compile a cross-compiler for your own](https://wiki.osdev.org/GCC_Cross-Compiler). Your system shall also have GCC tool chain installed, since we will use utility like GNU Make.

4. Hosted GCC & Binutils and Newlib for Simple-OS: We will need the specialized tool-chain and Newlib for those user space programs including init and shell. Please refer to `HostedToolchain.md` on how to build them.

5. (Optional) [VSCode](https://code.visualstudio.com/): We provide some integration of the building/debugging process with VSCode, but it is optional. Some extension may also be needed as described in the `Debug` section below.

## Build

### Configure

If your tool-chain is built by the `build-toolchain.sh` script, you can skip this section. The default values should work automatically.

Firstly, you need to change the `CROSSCOMPILERBIN` variable in `config.sh` to point it to the folder containing the cross-compiling GCC/Binutils binaries (see *Dependencies* section).

Note that the env variable `AS` is assumed to be the system wide NASM assembler, if not set, `nasm` is used.

Also, you need to make sure the hosted tool-chain and Newlib is compiled and installed to the location indicated by `TOOL_CHAIN_ROOT` variable in `config.sh` for the user space programs (those under `applications`) to be compiled successfully.

### Compile

Once you have the cross-compiler and hosted tool-chain ready, you can **build** Simple-OS by running:

```bash
./clean.sh
./build.sh
```

If compile finish successfully, `bootable_kernel.bin` will be generated under the source root dir.

### Emulate

You can test run the compiled kernel by QEMU:

```bash
./qemu.sh
```

Anything written to the serial port (all outputs to the screen will be copied to the serial port by the kernel) will be logged in `serial_port_output.txt`.

The script also check if there is a `testfs.fat` image file under root dir, if so, it will mount it as `hdb` when starting QEMU. This file should be a hard disk image of a FAT-32 file system. If `hdb` is present, Simple-OS will try to mount it under `/home`. For testing purpose, you can use the [FAT32 image]([testfs.fat.tar](https://github.com/aroulin/FAT32-FS-Driver/blob/master/testfs.fat.tar?raw=true)) provided by the [FAT32-FS-Driver](https://github.com/aroulin/FAT32-FS-Driver) project.

### Debug

It is possible to debug the kernel by GDB. See [QEMU GDB Usage](https://www.qemu.org/docs/master/system/gdb.html).

Start QEMU in WSL by:

```bash
./qemu.sh debug
```

And then attach GDB to the QEMU instance in WSL by:

```bash
gdb -ex "target remote localhost:1234" -ex "symbol-file sysroot/boot/simple_os.kernel"
```

The above loads debug symbols for the kernel, to debug our own bootloader:

```bash
gdb -ex "target remote localhost:1234" -ex "symbol-file bootloader/bootloader.elf" 
```

The `bootloader.elf` is generated solely to provide the debug symbols.

This trick is also describe in the [os-tutorial](https://github.com/cfenollosa/os-tutorial/tree/master/14-checkpoint) for macOS.

### VSCode Integration

Better still, it is possible to do all the above graphically in VSCode.

`.vscode/tasks.json` provide integrated Clean/Build/Emulate task.

You can trigger them in Command Palette (Ctrl+Shift+P).

`.vscode/launch.json` provides the debug gdb debug profiles.

Assuming you are running Simple-OS in a remote (virtual) machine, to setup the remote debugging:

1. [How to SSH to a system inside Virtual Box](https://medium.com/nycdev/how-to-ssh-from-a-host-to-a-guest-vm-on-your-local-machine-6cb4c91acc2e)
1. [Remote - SSH](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-ssh) extension
1. [Native Debug](https://marketplace.visualstudio.com/items?itemName=webfreak.debug) extension for remote debug through SSH.
1. VSCode extensions [Remote X11](https://marketplace.visualstudio.com/items?itemName=spadin.remote-x11) and [Remote X11 (SSH)](https://marketplace.visualstudio.com/items?itemName=spadin.remote-x11-ssh) to run QEMU (GUI application) through SSH
    - If your host machine is Windows, then a local X-window server is also needed. You can use [vcxsrv](https://sourceforge.net/projects/vcxsrv/).
    - If you SSH into a virtual machine by port forwarding, change `remoteX11.SSH.host` and `remoteX11.SSH.port` in "Remote X11 (SSH)" remote setting (e.g. change to 127.0.0.1 and 5679).

With all of the setup, the debugging process is streamlined to:

1. Set break points in any source file
1. Trigger `Build` task in Command Palette, make sure it runs successfully
1. Go to VSCode debugger, run `Debug All`
1. A QEMU session will be fired up and the system will run until a break point
1. After finishing debugging, click detach debugger and close the QEMU window

**Note:** Some code in `bootloader/arch/i386/bootloader.asm` and `kernel/arch/i386/boot/boot.asm` can not be debug in this way, please see the comments there.  

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

- `kernel/`
  - `block_io/`
    - Abstract layer for block storage
  - `console/`
    - A console/terminal emulator implemented a subset of VT-100 ASCII escape control sequence.
  - `elf/`
    - ELF binary parsing and loading
  - `fat/`
    - A full functioning FAT32 filesystem. See [Simple-FS](https://github.com/httpe/simple-fs)
  - `heap/`
    - [Heap]((http://www.jamesmolloy.co.uk/tutorial_html/7.-The%20Heap.html)) manager, allowing dynamic memory allocation and [virtual memory manager](https://wiki.osdev.org/Writing_a_memory_manager), implemented as a sorted linked list of free memory blocks with memory header and footer inspired by [INWOX](https://github.com/qvjp/INWOX/wiki/010-malloc()-&-free())
  - `include/`
    - Headers will be copied by `kernel`'s `make install` to `sysroot/` under project root folder, which will ultimately be packed into the disk image `bootable_kernel.bin` by `bootloader`'s Makefile, i.e. the headers will be shipped along with the kernel binary executable.
    - `arch/i386/`: Architecture specific headers
      - `port_io/`: Inline assembly for port I/O, e.g. inb/outb ([Examples](https://wiki.osdev.org/Inline_Assembly/Examples))
  - `kernel/`
    - `kernel.c`: Kernel main function
  - `memory_bitmap/`
    - A physical memory manager / page frame allocator implemented as a [bitmap](http://www.jamesmolloy.co.uk/tutorial_html/6.-Paging.html).
  - `panic/`
    - Utility functions to raise kernel panic
  - `tar/`
    - A simple US-TAR (tar ball) based read-only filesystem
  - `vfs/`
    - Virtual file system. An abstract layer to support mounting multiple device under a single rooted file tree.
  - `arch/i386/`: Architecture specific implementations
    - `arch_init/`: Collection of architecture specific initializations
    - `ata/`: 28 bit ATA PIO disk driver
    - `boot`:
      - `boot.asm`:
        - Intel/NASM syntax version of Meaty skeleton's boot.S
        - Plus initializing paging and [higher half kernel](https://wiki.osdev.org/Higher_Half_x86_Bare_Bones)
      - `gdt.asm`: Same as bootloader's flat mode GDT
    - `cpu/`: CPU related operations lick enable/disable interrupts
    - `crt/`: [C program initialization](https://wiki.osdev.org/Calling_Global_Constructors#Using_crti.o.2C_crtbegin.o.2C_crtend.o.2C_and_crtn.o_in_a_Kernel) components for the kernel
    - Interrupt related
      - `idt/`: [IDT](https://wiki.osdev.org/IDT) initialization and interrupt gate installation
      - `isr/`: [CPU exceptions](https://wiki.osdev.org/Exceptions) and IRQs handlers ([ISRs](https://wiki.osdev.org/Interrupt_Service_Routines))
      - `pic/`: [PIC](https://wiki.osdev.org/PIC) utility for IRQs
    - `keyboard/`: Keyboard driver
    - `paging/`: Virtual memory management and paging
    - `process/`:
      - `process.c`: Process management (Ref: [xv6/proc](https://github.com/mit-pdos/xv6-public/blob/master/proc.c))
      - `start_init.asm`: First usage mode program to run, will call SYS_EXEC to replace itself with `init.elf`
      - `switch_kernel_context.asm`: Used to switch between CPU context between scheduler (context which we initially boot into) and individual process's kernel code (e.g. system call handler)
    - `serial/`: [Serial port I/O](https://wiki.osdev.org/Serial_ports) utilities. We output all printing though serial port so QEMU can export the printed content to a file `serial_port_output.txt`.
    - `syscall/`: Wrapper for various system calls, mostly just recovering the argument from the stack.
    - `time/`: [RTC](https://wiki.osdev.org/RTC) clock
    - `timer/`: [PIT](https://wiki.osdev.org/PIT) timer (used for process switching)
    - `tty/` and `vga.h`: VGA Text Mode driver

- `libc/`: Progressive implementation of the standard C library
  - Mostly from Meaty Skeleton, and some are from Xv6.
  - Will be minimum in terms of feature set, since more complicated user space programs will be supported by ported external C library, like newlib.

### Applications

User space applications to be compiled by os specific tool-chain and use Newlib as their standard C library.

- `applications`:
  - `init/`
    - Compile to the first user space program to be ran (`init.elf`), perform user space initialization (like preparing for stdin/stdout/stderr) and then fork-exec shell. Currently also be used to test several user space features.
  - `shell/`
    - Shell, supporting commands include
      - `ls`
      - `cd`
      - and program execution
  - `editor/`
    - A port from the Kilo project
    - A simple editor with basic editing feature and C syntax highlighting

## Reference

Multiple tutorials have been referenced in the development of this project.

### OS Development Tutorials

1. [OsDev WikiTutorial Series](https://wiki.osdev.org/Tutorials): Very useful starting point of learning OS development. You can start with [Bare Bones](https://wiki.osdev.org/Bare_Bones) and then look into [Meaty Skeleton](https://wiki.osdev.org/Meaty_Skeleton), which we actual use in this project.

1. For tutorial in **Chinese**, I would recommend the [INWOX project](https://github.com/qvjp/INWOX/wiki).

1. [os-tutorial](https://github.com/cfenollosa/os-tutorial): A very user friendly step by step OS dev tutorial. Many files of the bootloader or the kernel are modified from this repo. This tutorial also combines a lot of pieces from some of the following tutorials. In addition, this tutorial assumes a **macOS** environment.

1. [Roll your own toy UNIX-clone OS](http://www.jamesmolloy.co.uk/tutorial_html/) a well known tutorial, very concise and easy to understand. You shall read it with the [errata](https://wiki.osdev.org/James_Molloy%27s_Tutorial_Known_Bugs) at hand.

1. [The little book about OS development](http://littleosbook.github.io/#) and [Writing a Simple Operating System - from Scratch](https://www.cs.bham.ac.uk/~exr/lectures/opsys/10_11/lectures/os-dev.pdf). They provide explanation to exemplary code for many OS concepts. They can be used as a complement to the OSDev Wiki.

1. [MIT 6.S081 Operating System Engineering Course](https://pdos.csail.mit.edu/6.828/2020/schedule.html). This course provides lecture video this year so it can be seen as a good OS MOOC. It is based on a very popular educational system Xv6, which is a full functioning multi-process OS implemented within 10K lines of code. It will serve as our reference implementation. The course switched to RISC-V architecture in 2019 ([Xv6-RISC](https://github.com/mit-pdos/xv6-riscv)), however for our purpose, the [x86 version](https://github.com/mit-pdos/xv6-public) will be more useful. It also provides a [functional classification](https://pdos.csail.mit.edu/6.828/2018/xv6/xv6-rev10.pdf) for each file in the x86 version repo.

1. [Build Your Own Text Editor](https://viewsourcecode.org/snaptoken/kilo/index.html) A very good step by step tutorial on how to build a text application like an editor from scratch. The final product, Kilo, is around 1000 lines of code, and includes features like basic text editing, searching and even C syntax highlighting.

### Documentation

1. [NASM Documentation](https://www.nasm.us/xdoc/2.15.05/html/nasmdoc0.html)
1. [GNU Make Documentation](https://www.gnu.org/software/make/manual/make.html)
1. [ELF Format](http://www.skyfree.org/linux/references/ELF_Format.pdf)
1. [x86 Instruction Set Manual](https://www.felixcloutier.com/x86/)
1. [GCC Inline Assembly](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html)
