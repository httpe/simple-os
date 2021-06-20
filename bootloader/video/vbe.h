#ifndef _VBE_H
#define _VBE_H

#include <stdint.h>

// Structure returned by VBE BIOS function 0x4F00
// Number after colon is offset into the struct
typedef struct vbe_info_structure {
    char signature[4];	        // must be "VESA" to indicate valid VBE support: 0
    uint16_t version;			// VBE version; high byte is major version, low byte is minor version: 4
    uint32_t oem;			    // segment:offset pointer to OEM: 6
    uint32_t capabilities;		// bitfield that describes card capabilities: 10
    uint32_t video_modes;		// segment:offset pointer to list of supported video modes: 14
    uint16_t video_memory;		// amount of video memory in 64KB blocks: 18
    uint16_t software_rev;		// software revision: 20
    uint32_t vendor;			// segment:offset to card vendor string: 22
    uint32_t product_name;		// segment:offset to card model name: 26
    uint32_t product_rev;		// segment:offset pointer to product revision: 30
    char reserved[222];		    // reserved for future expansion: 34
    char oem_data[256];		    // OEM BIOSes store their strings in this area: 256
} __attribute__ ((packed)) vbe_info_structure;

// Structure returned by VBE BIOS function 0x4F01
// Number after colon is offset into the struct
typedef struct vbe_mode_info_structure {
    uint16_t attributes;		// deprecated, only bit 7 should be of interest to you, and it indicates the mode supports a linear frame buffer.: 0
    uint8_t window_a;			// deprecated: 2
    uint8_t window_b;			// deprecated: 3
    uint16_t granularity;		// deprecatedused while calculating bank numbers: 4
    uint16_t window_size;       // : 6
    uint16_t segment_a;         // : 8
    uint16_t segment_b;         // : 10
    uint32_t win_func_ptr;		// deprecatedused to switch banks from protected mode without returning to real mode: 12
    uint16_t pitch;			    // number of bytes per horizontal line: 16
    uint16_t width;			    // width in pixels: 18
    uint16_t height;			// height in pixels: 20
    uint8_t w_char;			    // unused...: 22
    uint8_t y_char;			    // ...: 23
    uint8_t planes;             // : 24
    uint8_t bpp;			    // bits per pixel in this mode: 25
    uint8_t banks;			    // deprecatedtotal number of banks in this mode: 26
    uint8_t memory_model;       // : 27
    uint8_t bank_size;		    // deprecatedsize of a bank, almost always 64 KB but may be 16 KB...: 28
    uint8_t image_pages;        // : 29
    uint8_t reserved0;          // : 30

    uint8_t red_mask;           // : 31
    uint8_t red_position;       // : 32
    uint8_t green_mask;         // : 33
    uint8_t green_position;     // : 34
    uint8_t blue_mask;          // : 35
    uint8_t blue_position;      // : 36
    uint8_t reserved_mask;      // : 37
    uint8_t reserved_position;  // : 38
    uint8_t direct_color_attributes;   // : 39

    uint32_t framebuffer;		        // physical address of the linear frame bufferwrite here to draw to the screen: 40
    uint32_t off_screen_mem_off;        // : 44
    uint16_t off_screen_mem_size;	    // size of memory in the framebuffer but not being displayed on the screen : 48
    uint8_t reserved1[206];             // : 50
} __attribute__((packed)) vbe_mode_info_structure;



#endif