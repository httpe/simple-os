
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../string/string.h"
#include "../tar/tar.h"
#include "../elf/elf.h"
#include "../multiboot/multiboot.h"
#include "../video/video.h"

#include "../arch/i386/ata.h"
#include "../arch/i386/tty.h"

// The whole bootloader binary is assumed to take the first 16 sectors
// must be in sync of BOOTLOADER_MAX_SIZE in Makefile
#define BOOTLOADER_SECTORS 32
#define KERNEL_BOOT_IMG "/boot/simple_os.kernel"

// Defined in memory.asm
extern uint16_t MMAP_COUNT;
extern multiboot_memory_map_t MMAP[];


static const char* VGA_FONT_MODULE_CMDLINE = "VGA FONT";

// Load a file from the tar file system
//
// LBA: 0-based Linear Block Address, 28bit LBA shall be between 0 to 0x0FFFFFFF
// filename: get a file with this name from the tar file system
// buffer: load the file found into this address
//
// return: file size of the loaded file
int tar_loopup_lazy(uint32_t LBA, char* filename, unsigned char* buffer) {
    uint32_t max_lba = get_total_28bit_sectors();
    if (LBA >= max_lba) {
        return TAR_ERR_LBA_GT_MAX_SECTOR;
    }
    read_sectors_ATA_PIO(buffer, LBA, 1);
    int match = tar_match_filename(buffer, filename);
    if (match == TAR_ERR_NOT_USTAR) {
        return TAR_ERR_NOT_USTAR;
    } else {
        int filesize = tar_get_filesize(buffer);
        int size_in_sector = ((filesize + 511) / 512) + 1; // plus one for the meta sector
        if (match == TAR_ERR_FILE_NAME_NOT_MATCH) {
            return tar_loopup_lazy(LBA + size_in_sector, filename, buffer);
        } else {
            read_sectors_ATA_PIO(buffer, LBA + 1, size_in_sector);
            return filesize;
        }
    }

}

void bootloader_main(void) {

    // Init multiboot info structure and memory map info
    // Ref: https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
    // Store the Multiboot info structure at the end of conventional memory space 0x00007E00 - 0x0007FFFF
    multiboot_info_t* ptr_multiboot_info = (multiboot_info_t*)0x00080000 - sizeof(multiboot_info_t);
    ptr_multiboot_info->flags = 
        MULTIBOOT_INFO_MEM_MAP | MULTIBOOT_INFO_VBE_INFO | MULTIBOOT_INFO_FRAMEBUFFER_INFO | MULTIBOOT_INFO_MODS;
    ptr_multiboot_info->mods_count = 0;
    ptr_multiboot_info->mods_addr = ((uint32_t) ptr_multiboot_info);


    // Save memory map info to Multiboot structure
    // ptr_multiboot_info->mmap_length = (*(uint32_t*)ADDR_MMAP_COUNT) * sizeof(multiboot_memory_map_t);
    // ptr_multiboot_info->mmap_addr = ADDR_MMAP_ADDR;
    ptr_multiboot_info->mmap_length = MMAP_COUNT * sizeof(multiboot_memory_map_t);
    ptr_multiboot_info->mmap_addr = (uint32_t) MMAP;

    // Save VBE video BIOS info to Multiboot structure
    // Please see "VBE info" section at https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
    if(VESA_MODE != 0) {
        // VBE Video mode
        ptr_multiboot_info->framebuffer_type = MULTIBOOT_FRAMEBUFFER_TYPE_RGB;
        ptr_multiboot_info->vbe_control_info = (uint32_t) VESA_BIOS_INFO;
        ptr_multiboot_info->vbe_mode_info = (uint32_t) VESA_MODE_INFO;
        ptr_multiboot_info->vbe_mode = VESA_MODE;
        ptr_multiboot_info->framebuffer_addr = VESA_MODE_INFO->framebuffer;
        ptr_multiboot_info->framebuffer_pitch = VESA_MODE_INFO->pitch;
        ptr_multiboot_info->framebuffer_width = VESA_MODE_INFO->width;
        ptr_multiboot_info->framebuffer_height = VESA_MODE_INFO->height;
        ptr_multiboot_info->framebuffer_bpp = VESA_MODE_INFO->bpp;
        
        ptr_multiboot_info->mods_count++;
        ptr_multiboot_info->mods_addr -= sizeof(struct multiboot_mod_list);
        struct multiboot_mod_list* mod = (struct multiboot_mod_list*) ptr_multiboot_info->mods_addr;
        mod->mod_start = VGA_FONT_ADDR;
        mod->mod_end = VGA_FONT_ADDR + 16*8*256;
        mod->cmdline = (uint32_t) VGA_FONT_MODULE_CMDLINE;

        init_video(ptr_multiboot_info);
        // Display graphical welcome message if in VGA video mode
        uint32_t bgcolor = 0x0066CCFF;
        uint32_t fgcolor = 0x00FFFFFF;
        fillrect(bgcolor, 200, 200, 400, 200);
        char* hello_msg = "Welcome to 32-bit Simple-Bootloader!";
        int x = 400 - strlen(hello_msg) / 2 * 8;
        int y = 300 - 16 / 2;
        for(unsigned int i=0; i<strlen(hello_msg); i++) {
            drawchar(hello_msg[i], x + 8*i, y, bgcolor, fgcolor);
        }
    } else {
        // EGA Text Mode (80 * 40)
        ptr_multiboot_info->framebuffer_type = MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT;
        ptr_multiboot_info->vbe_control_info = 0;
        ptr_multiboot_info->vbe_mode_info = 0;
        ptr_multiboot_info->vbe_mode = 0;
        ptr_multiboot_info->framebuffer_addr = 0xB8000;
        ptr_multiboot_info->framebuffer_pitch = 160;
        ptr_multiboot_info->framebuffer_width = 80;
        ptr_multiboot_info->framebuffer_height = 25;
        ptr_multiboot_info->framebuffer_bpp = 16;
    }

    init_tty(ptr_multiboot_info);

    uint8_t console_print_row = 0;

    // Load kernel from tar file system to this address
    char* kernel_buffer = (char*)0x01000000;
    int kernel_size = tar_loopup_lazy(BOOTLOADER_SECTORS, KERNEL_BOOT_IMG, (unsigned char*)kernel_buffer);
    print_str("Kernel Image Loaded at (Little Endian Hex):", console_print_row++, 0);
    print_memory_hex((char*)&kernel_buffer, sizeof(kernel_buffer), console_print_row++);
    print_str("Kernel size (Little Endian Hex):", console_print_row++, 0);
    print_memory_hex((char*)&kernel_size, sizeof(int), console_print_row++);
    // print_str("First 32 bytes of kernel:", console_print_row++, 0);
    // print_memory(kernel_buffer, 32, console_print_row++, 0);

    if (is_elf(kernel_buffer)) {
        // print_str("ELF Kernel Detected, now loading...", console_print_row++, 0);
        Elf32_Addr entry_point_physical = load_elf(kernel_buffer);
        // print_str("ELF Kernel first 16 bytes:", console_print_row++, 0);
        // print_memory_hex((char*) entry_point_physical, 16, console_print_row++);
        print_str("Jumping to ELF Kernel physical entry point (Little Endian Hex):", console_print_row++, 0);
        print_memory_hex((char*)&entry_point_physical, sizeof(entry_point_physical), console_print_row++);

        // Jump to the physical entry point of the loaded ELF kernel image 
        void* entry_point = (void*)entry_point_physical;

        // Multiboot convention: use ebx to store the pointer to the multiboot info structure
        print_str("Multiboot structure saved at (Little Endian Hex):", console_print_row++, 0);
        print_memory_hex((char*)&ptr_multiboot_info, sizeof(ptr_multiboot_info), console_print_row++);
        asm volatile("mov %0, %%ebx":: "r"(ptr_multiboot_info));

        goto *entry_point;
    }

}