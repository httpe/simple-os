#include <kernel/video.h>
#include <kernel/paging.h>
#include <kernel/lock.h>
#include <kernel/panic.h>
#include <string.h>
#include <stdio.h>
#include <arch/i386/kernel/cpu.h>

// Ref: https://wiki.osdev.org/Drawing_In_Protected_Mode
// Ref: https://wiki.osdev.org/Getting_VBE_Mode_Info
// Ref: https://wiki.osdev.org/User:Omarrx024/VESA_Tutorial
// Ref: https://wiki.osdev.org/Double_Buffering
// Ref: https://wiki.osdev.org/VGA_Fonts
// Ref: http://www.petesqbsite.com/sections/tutorials/tuts/vbe3.pdf

// Good advices on improving video performance
// Scrolling in the VESA mode: https://forum.osdev.org/viewtopic.php?f=1&t=22702
// Sluggish SVGA drivers: https://forum.osdev.org/viewtopic.php?f=15&t=22882
// gui scroll is super slow: https://forum.osdev.org/viewtopic.php?f=1&t=23891&start=0 (in which Brendan suggested this buffer_curr/buffer_next method)

static struct {
  int initialized;
  uint32_t* framebuffer;
  uint32_t* buffer_curr;
  uint32_t* buffer_next;
  uint32_t buffer_byte_size;
  uint32_t buffer_pixel_size;
  uint8_t bpp;
  uint32_t pitch;
  uint32_t width;
  uint32_t height;
  uint8_t* font;
  yield_lock lk;
} video;


void putpixel(uint32_t color, int x, int y) {
    if(!video.initialized) return;
    acquire(&video.lk);
    video.buffer_next[x + y*video.width] = color;
    release(&video.lk);
}

// void putpixel24(uint32_t color, int x, int y) {
//     uint32_t where = x + y*pitch_in_pixel;
//     framebuffer[where] = color & 0xFF;
//     framebuffer[where+1] = (color >> 8) & 0xFF;
//     framebuffer[where+2] = (color >> 16) & 0xFF;
// }

void video_refresh()
{
    if(!video.initialized) return;
    acquire(&video.lk);
    // only write changed pixels to video memory
    for(uint i=0; i<video.buffer_pixel_size; i++) {
        if(video.buffer_next[i] != video.buffer_curr[i]) {
            video.buffer_curr[i] = video.buffer_next[i];
            video.framebuffer[i] = video.buffer_next[i];
        }
    }
    release(&video.lk);
}

void video_refresh_rect(uint x, uint y, uint w, uint h)
{
    if(!video.initialized) return;
    uint off = y*video.width + x;
    uint i,j;
    acquire(&video.lk);
    for(i=0; i<h;i++) {
        for(j=0; j<w;j++) {
            if(video.buffer_next[off] != video.buffer_curr[off]) {
                video.buffer_curr[off] = video.buffer_next[off];
                video.framebuffer[off] = video.buffer_next[off];
            }
            off++;
        }
        off += video.width - w;
    }
    release(&video.lk);
}

void fillrect(uint32_t color, int x, int y, int w, int h) {
    if(!video.initialized) return;
    uint32_t* where = &video.buffer_next[x + y*video.width];
    int i, j;
    acquire(&video.lk);
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            *where++ = color;
        }
        where += video.width - w;
    }
    release(&video.lk);
}

void drawpic(uint32_t* buff, int x, int y, int w, int h) {
    if(!video.initialized) return;
    if(x >= (int) video.width || y >= (int) video.height) {
        return;
    }
    if(x + w > (int) video.width) {
        w = video.width - x;
    }
    if(y + h > (int) video.height) {
        h = video.height - y;
    }
    uint32_t* where = &video.buffer_next[x + y*video.width];
    int i, j;    
    acquire(&video.lk);
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            *where++ = *buff++;
        }
        where += video.width - w;
    }
    release(&video.lk);
}
 
void drawchar(unsigned char c, int x, int y, uint32_t bgcolor, uint32_t fgcolor)
{
    if(!video.initialized) return;
    int cy;
    int mask[8]={128,64,32,16,8,4,2,1}; // should match FONT_WIDTH
	unsigned char *glyph=video.font+(int)c*FONT_HEIGHT;
    uint32_t* buff = &video.buffer_next[x + y*video.width];
    acquire(&video.lk);
	for(cy=0;cy<FONT_HEIGHT;cy++){
        // optimized but only usable for font width == 8
        *buff++ = glyph[cy]&mask[0]?fgcolor:bgcolor;
        *buff++ = glyph[cy]&mask[1]?fgcolor:bgcolor;
        *buff++ = glyph[cy]&mask[2]?fgcolor:bgcolor;
        *buff++ = glyph[cy]&mask[3]?fgcolor:bgcolor;
        *buff++ = glyph[cy]&mask[4]?fgcolor:bgcolor;
        *buff++ = glyph[cy]&mask[5]?fgcolor:bgcolor;
        *buff++ = glyph[cy]&mask[6]?fgcolor:bgcolor;
        *buff++ = glyph[cy]&mask[7]?fgcolor:bgcolor;
		// for(cx=0;cx<FONT_WIDTH;cx++){
        //     *buff++ = glyph[cy]&mask[cx]?fgcolor:bgcolor;   
		// }
        buff += video.width - FONT_WIDTH;
	}
    release(&video.lk);
}

void screen_scroll_up(uint32_t row, uint32_t bgcolor)
{
    if(!video.initialized) return;
    acquire(&video.lk);
    memmove(video.buffer_next, ((void*) video.buffer_next) + row * video.pitch, (video.height - row)*video.pitch);
    release(&video.lk);
    fillrect(bgcolor, 0, video.height - row, video.width, row);
}


void clear_screen(uint32_t bgcolor) {
    fillrect(bgcolor, 0, 0, video.width, video.height);
}

void get_screen_size(uint32_t* w, uint32_t* h)
{
    *w = video.width;
    *h = video.height;
}

void get_vga_font_size(uint32_t* w, uint32_t* h)
{
    *w = FONT_WIDTH;
    *h = FONT_HEIGHT;
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
            video.font = (uint8_t*) (mods->mod_start + 0xC0000000);
            // assert mapping is expected
            PANIC_ASSERT(vaddr2paddr(curr_page_dir(), (uint32_t) video.font) == mods->mod_start);
            break;
        }
    }
    if(video.font == NULL) {
        PANIC("No VGA font");
    }
    
    if(info->framebuffer_bpp != 32) {
        // hang assert the color depth must be 32 bits
        PANIC("VGA BPP != 32");
    }

    video.bpp = info->framebuffer_bpp;
    video.pitch = info->framebuffer_pitch;
    video.width = info->framebuffer_width;
    video.height = info->framebuffer_height;
    video.buffer_byte_size = video.width*video.height*video.bpp/8;
    video.buffer_pixel_size = video.width*video.height;

    // we assume here and in tty that there is no padding between lines
    PANIC_ASSERT(video.pitch == video.bpp/8 * video.width);
    // drawchar uses this assumption
    PANIC_ASSERT(FONT_WIDTH == 8);

    video.framebuffer = (uint32_t*) (uint32_t) info->framebuffer_addr;
    if(!is_vaddr_accessible(curr_page_dir(), (uint32_t) video.framebuffer, true, true)) {
        // if not mapped, do an identify mapping
        uint32_t frame_idx = PAGE_INDEX_FROM_VADDR((uint32_t) video.framebuffer);
        map_pages_at(
            curr_page_dir(), 
            PAGE_INDEX_FROM_VADDR((uint32_t) video.framebuffer), 
            PAGE_COUNT_FROM_BYTES(video.buffer_byte_size), 
            &frame_idx,
            true,
            true,
            true
            );
    }
    // assert the mapping is identity
    PANIC_ASSERT(vaddr2paddr(curr_page_dir(), (uint32_t) video.framebuffer) == (uint32_t) video.framebuffer);
    PANIC_ASSERT(
        vaddr2paddr(curr_page_dir(), (uint32_t) video.framebuffer + (video.buffer_byte_size - 1)) 
        == 
        (uint32_t) video.framebuffer + (video.buffer_byte_size - 1)
        );

    // Use two video buffers, buffer_curr holds the current view and buffer_next holds the next view to be displayed/flushed to framebuffer
    video.buffer_curr = (uint32_t*) alloc_pages_consecutive_frames(curr_page_dir(), PAGE_COUNT_FROM_BYTES(video.buffer_byte_size), true, NULL);
    memset(video.buffer_curr, 0, PAGE_COUNT_FROM_BYTES(video.buffer_byte_size) * PAGE_SIZE);
    video.buffer_next = (uint32_t*) alloc_pages_consecutive_frames(curr_page_dir(), PAGE_COUNT_FROM_BYTES(video.buffer_byte_size), true, NULL);
    memset(video.buffer_next, 0, PAGE_COUNT_FROM_BYTES(video.buffer_byte_size) * PAGE_SIZE);
    // sync video buffer
    memmove(video.framebuffer, video.buffer_curr, video.buffer_byte_size);

    video.initialized = 1;

    return 0;
}
