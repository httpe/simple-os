#include "video.h"
#include "../string/string.h"

static int initialized;

static uint32_t* framebuffer;
static uint8_t bpp;
static uint32_t pitch;
static uint32_t width;
static uint32_t height;
static uint8_t* font;

#define FONT_HEIGHT 16
#define FONT_WIDTH 8

void putpixel(uint32_t color, int x, int y) {
    framebuffer[x + y*width] = color;
}

// void putpixel24(uint32_t color, int x, int y) {
//     uint32_t where = x + y*pitch_in_pixel;
//     framebuffer[where] = color & 0xFF;
//     framebuffer[where+1] = (color >> 8) & 0xFF;
//     framebuffer[where+2] = (color >> 16) & 0xFF;
// }

void fillrect(uint32_t color, int x, int y, int w, int h) {
    uint32_t* where = &framebuffer[x + y*width];
    int i, j;
 
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            *where++ = color;
        }
        where += width - w;
    }
}
 
void drawchar(unsigned char c, int x, int y, uint32_t bgcolor, uint32_t fgcolor)
{
    int cx,cy;
    int mask[8]={128,64,32,16,8,4,2,1}; // should match FONT_WIDTH
	unsigned char *glyph=font+(int)c*FONT_HEIGHT;

	for(cy=0;cy<FONT_HEIGHT;cy++){
		for(cx=0;cx<FONT_WIDTH;cx++){
			putpixel(glyph[cy]&mask[cx]?fgcolor:bgcolor,x+cx,y+cy);
		}
	}
}

void drawchar_textmode(unsigned char c, int char_x, int char_y, uint32_t bgcolor, uint32_t fgcolor)
{
    drawchar(c, char_x * FONT_WIDTH, char_y * FONT_HEIGHT, bgcolor, fgcolor);
}

void get_textmode_screen_size(uint32_t* w_in_char, uint32_t* h_in_char)
{
    *w_in_char = width / FONT_WIDTH;
    *h_in_char = height / FONT_HEIGHT;
}

int init_video(multiboot_info_t* info) 
{
    if(!((info->flags & MULTIBOOT_INFO_VBE_INFO) && (info->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB))) {
        return -1;
    }
    
    // Load VGA font from boot module
    if(!(info->flags & MULTIBOOT_INFO_MODS)) {
        // must have VBE info and boot modules
        while (1);
    }
    struct multiboot_mod_list* mods = (struct multiboot_mod_list*) info->mods_addr;
    for(uint32_t i=0; i<info->mods_count; i++) {
        // find boot module of BIOS VGA font
        if(memcmp((char*) mods[i].cmdline, "VGA FONT", 9) == 0 && mods[i].mod_end - mods[i].mod_start == 256*FONT_WIDTH*FONT_HEIGHT) {
            font = (uint8_t*) mods->mod_start;
            break;
        }
    }
    if(font == NULL) {
        while (1);
    }
    
    if(info->framebuffer_bpp != 32) {
        // hang assert the color depth must be 32 bits
        while (1);
    }

    framebuffer = (uint32_t*) (uint32_t) info->framebuffer_addr;
    bpp = info->framebuffer_bpp;
    pitch = info->framebuffer_pitch;
    width = info->framebuffer_width;
    height = info->framebuffer_height;

    initialized = 1;

    return 0;
}
