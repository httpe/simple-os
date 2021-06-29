#ifndef _VIDEO_H
#define _VIDEO_H

#include <stdint.h>
#include "vbe.h"
#include "../multiboot/multiboot.h"

// Defined in vesa.asm
extern uint16_t VESA_MODE, VGA_FONT_ADDR;
extern vbe_info_structure VESA_BIOS_INFO[];
extern vbe_mode_info_structure VESA_MODE_INFO[];

void putpixel(uint32_t color, int x, int y);
void fillrect(uint32_t color, int x, int y, int w, int h);
void drawchar(unsigned char c, int x, int y, uint32_t bgcolor, uint32_t fgcolor);
void drawchar_textmode(unsigned char c, int char_x, int char_y, uint32_t bgcolor, uint32_t fgcolor);
void get_textmode_screen_size(uint32_t* w_in_char, uint32_t* h_in_char);
int init_video(multiboot_info_t* info);


#endif