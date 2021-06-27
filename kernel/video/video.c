#include <string.h>
#include <kernel/video.h>
#include <kernel/paging.h>
#include <kernel/panic.h>

#include <arch/i386/kernel/cpu.h>

static int initialized;

static uint32_t* framebuffer;
static uint32_t* video_buffer;
static uint32_t buffer_byte_size;
static uint32_t buffer_pixel_size;
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


void swap_buffer(uint32_t pixel_idx, uint32_t len) {
    memmove(framebuffer + pixel_idx, video_buffer + pixel_idx, len*bpp/8);
}

void video_refresh() {
    swap_buffer(0, buffer_pixel_size);
}

void fillrect(uint32_t color, int x, int y, int w, int h) {
    uint32_t* where = &video_buffer[x + y*width];
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
    uint32_t* buff = &video_buffer[x + y*width];
	for(cy=0;cy<FONT_HEIGHT;cy++){
		for(cx=0;cx<FONT_WIDTH;cx++){
            *buff++ = glyph[cy]&mask[cx]?fgcolor:bgcolor;
		}
        buff += width - FONT_WIDTH;
	}
    
}

void drawchar_textmode(unsigned char c, int char_x, int char_y, uint32_t bgcolor, uint32_t fgcolor)
{
    drawchar(c, char_x * FONT_WIDTH, char_y * FONT_HEIGHT, bgcolor, fgcolor);
    uint32_t off =  char_y * FONT_HEIGHT * width + char_x * FONT_WIDTH;
    for(uint i=0; i<FONT_HEIGHT;i++) {
        swap_buffer(off, FONT_WIDTH);
        off += width;
    }
}

void screen_scroll_up(uint32_t row, uint32_t bgcolor)
{
    memmove(video_buffer, ((void*) video_buffer) + row * pitch, (height - row)*pitch);
    fillrect(bgcolor, 0, height - row, width, row);
}

void scroll_up_textmode(uint32_t row, uint32_t bgcolor)
{
    screen_scroll_up(row * FONT_HEIGHT, bgcolor);
    // clear_textmode_screen_edge(bgcolor);
    video_refresh();
}

// Fill screen right/bottom edges which doesn't covered by textmode characters 
// static void clear_textmode_screen_edge(uint32_t bgcolor)
// {
//     uint32_t width_rem = width % FONT_WIDTH;
//     if(width_rem > 0) {
//         fillrect(bgcolor, width - width_rem - 1, 0, width_rem, height);
//     }
//     uint32_t height_rem = height % FONT_HEIGHT;
//     if(height_rem > 0) {
//         fillrect(bgcolor, 0, height - height_rem - 1, width_rem, height_rem);
//     }
// }

void clear_screen(uint32_t bgcolor) {
    fillrect(bgcolor, 0, 0, width, height);
}

void get_textmode_screen_size(uint32_t* w_in_char, uint32_t* h_in_char)
{
    *w_in_char = width / FONT_WIDTH;
    *h_in_char = height / FONT_HEIGHT;
}

int init_video(uint32_t info_phy_addr)
{
    multiboot_info_t* info = (multiboot_info_t*) (info_phy_addr + 0xC0000000);

    if(!((info->flags & MULTIBOOT_INFO_VBE_INFO) && (info->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB))) {
        return -1;
    }
    
    // Load VGA font from boot module
    if(!(info->flags & MULTIBOOT_INFO_MODS)) {
        PANIC("Must have VBE info and boot modules");
    }
    struct multiboot_mod_list* mods = (struct multiboot_mod_list*) (info->mods_addr + 0xC0000000);
    // assert mapping is expected
    PANIC_ASSERT(vaddr2paddr(curr_page_dir(), (uint32_t) mods) == info->mods_addr);
    for(uint32_t i=0; i<info->mods_count; i++) {
        // find boot module of BIOS VGA font
        if(memcmp((char*) (mods[i].cmdline + 0xC0000000), "VGA FONT", 9) == 0 && mods[i].mod_end - mods[i].mod_start == 256*FONT_WIDTH*FONT_HEIGHT) {
            font = (uint8_t*) (mods->mod_start + 0xC0000000);
            // assert mapping is expected
            PANIC_ASSERT(vaddr2paddr(curr_page_dir(), (uint32_t) font) == mods->mod_start);
            break;
        }
    }
    if(font == NULL) {
        PANIC("No VGA font");
    }
    
    if(info->framebuffer_bpp != 32) {
        // hang assert the color depth must be 32 bits
        PANIC("VGA BPP != 32");
    }

    bpp = info->framebuffer_bpp;
    pitch = info->framebuffer_pitch;
    width = info->framebuffer_width;
    height = info->framebuffer_height;
    buffer_byte_size = width*height*bpp/8;
    buffer_pixel_size = width*height;

    PANIC_ASSERT(pitch == bpp/8 * width);

    framebuffer = (uint32_t*) (uint32_t) info->framebuffer_addr;
    if(!is_vaddr_accessible(curr_page_dir(), (uint32_t) framebuffer, true, true)) {
        // if not mapped, do an identify mapping
        uint32_t frame_idx = PAGE_INDEX_FROM_VADDR((uint32_t) framebuffer);
        map_pages_at(
            curr_page_dir(), 
            PAGE_INDEX_FROM_VADDR((uint32_t) framebuffer), 
            PAGE_COUNT_FROM_BYTES(width*height*bpp/8), 
            &frame_idx,
            true,
            true,
            true
            );
    }
    // assert the mapping is identity
    PANIC_ASSERT(vaddr2paddr(curr_page_dir(), (uint32_t) framebuffer) == (uint32_t) framebuffer);
    PANIC_ASSERT(vaddr2paddr(curr_page_dir(), (uint32_t) framebuffer + (width*height*bpp/8 - 1)) == (uint32_t) framebuffer + (width*height*bpp/8 - 1));

    video_buffer = (uint32_t*) alloc_pages_consecutive_frames(curr_page_dir(), PAGE_COUNT_FROM_BYTES(width*height*bpp/8), true, NULL);
    memset(video_buffer, 0, buffer_byte_size);
    swap_buffer(0, buffer_pixel_size);

    initialized = 1;

    // Performance benchmark
    // uint64_t t0 = rdtsc();

    // for(int y=0;y < 1;y++) {
    //     for(int x=0;x < 100; x++) {
    //         drawchar_textmode('X', x, y, 0x0, 0x00FFFFFF);
    //     }
    // }

    // uint64_t t1 = rdtsc();
    // uint64_t d = t1 - t0;

    return 0;
}
