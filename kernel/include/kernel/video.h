#ifndef _KERNEL_VIDEO_H
#define _KERNEL_VIDEO_H

#include <stdint.h>
#include <common.h>
#include <kernel/vbe.h>
#include <kernel/multiboot.h>

void putpixel(uint32_t color, int x, int y);
void fillrect(uint32_t color, int x, int y, int w, int h);
void drawpic(uint32_t* buff, int x, int y, int w, int h);
void screen_scroll_up(uint32_t row, uint32_t bgcolor);
void clear_screen(uint32_t bgcolor);

void video_refresh();
void video_refresh_rect(uint x, uint y, uint w, uint h);

void drawchar(unsigned char c, int x, int y, uint32_t bgcolor, uint32_t fgcolor);

void get_screen_size(uint32_t* w, uint32_t* h);
void get_vga_font_size(uint32_t* w, uint32_t* h);

int init_video(uint32_t info_phy_addr);


#endif