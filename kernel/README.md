# README

Please see <https://wiki.osdev.org/Meaty_Skeleton#Cross-Compiling_the_Operating_System> for details.

## Cross-Compiling the Operating System

The system is cross-compiled in the same manner as Bare Bones, though with the complexity of having a system root with the final system and using a libk. In this example, we elected to use shell scripts to to the top-level build process, though you could possibly also use a makefile for that or a wholly different build system. Though, assuming this setup works for you, you can clean the source tree by invoking:

```bash
./clean.sh
```

You can install all the system headers into the system root without relying on the compiler at all, which will be useful later on when switching to a Hosted GCC Cross-Compiler, by invoking:

```bash
./headers.sh
```

You can build a bootable cdrom image of the operating system by invoking:

```bash
./iso.sh
```

QEMU needs GTK, so we will rather run the QEMU from Windows PowerShell

```ps
qemu-system-i386.exe -cdrom .\myos.iso
```
