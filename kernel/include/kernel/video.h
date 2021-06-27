#ifndef _KERNEL_VIDEO_H
#define _KERNEL_VIDEO_H

#include <stdint.h>
#include <kernel/vbe.h>
#include <kernel/multiboot.h>

void putpixel(uint32_t color, int x, int y);
void fillrect(uint32_t color, int x, int y, int w, int h);
void screen_scroll_up(uint32_t row, uint32_t bgcolor);
void clear_screen(uint32_t bgcolor);

void video_refresh();

void drawchar(unsigned char c, int x, int y, uint32_t bgcolor, uint32_t fgcolor);
void drawchar_textmode(unsigned char c, int char_x, int char_y, uint32_t bgcolor, uint32_t fgcolor);
void get_textmode_screen_size(uint32_t* w_in_char, uint32_t* h_in_char);
void scroll_up_textmode(uint32_t row, uint32_t bgcolor);

int init_video(uint32_t info_phy_addr);


#endif