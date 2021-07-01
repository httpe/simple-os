#include <string.h>
#include <kernel/video.h>
#include <kernel/paging.h>
#include <kernel/panic.h>


// Ref: https://wiki.osdev.org/Drawing_In_Protected_Mode
// Ref: https://wiki.osdev.org/Getting_VBE_Mode_Info
// Ref: https://wiki.osdev.org/User:Omarrx024/VESA_Tutorial
// Ref: https://wiki.osdev.org/Double_Buffering
// Ref: https://wiki.osdev.org/VGA_Fonts
// Ref: http://www.petesqbsite.com/sections/tutorials/tuts/vbe3.pdf

// Good advices on improving video performance
// Scrolling in the VESA mode: https://forum.osdev.org/viewtopic.php?f=1&t=22702
// Sluggish SVGA drivers: https://forum.osdev.org/viewtopic.php?f=15&t=22882
// gui scroll is super slow: https://forum.osdev.org/viewtopic.php?f=1&t=23891&start=0 (in which Brendan suggested this video_buffer_curr/video_buffer_next method)

static int initialized;

static uint32_t* framebuffer;
static uint32_t* video_buffer_curr;
static uint32_t* video_buffer_next;
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


// static void swap_buffer(uint32_t pixel_idx, uint32_t len) {
//     if(stop_swap) {
//         return;
//     }
//     memmove(framebuffer + pixel_idx, video_buffer_next + pixel_idx, len*bpp/8);
// }

void video_refresh()
{
    // only write changed pixels to video memory
    for(uint i=0; i<buffer_pixel_size; i++) {
        if(video_buffer_next[i] != video_buffer_curr[i]) {
            video_buffer_curr[i] = video_buffer_next[i];
            framebuffer[i] = video_buffer_next[i];
        }
    }
}
void video_refresh_rect(uint x, uint y, uint w, uint h)
{
    uint off = y*width + x;
    uint i,j;
    for(i=0; i<h;i++) {
        for(j=0; j<w;j++) {
            if(video_buffer_next[off] != video_buffer_curr[off]) {
                video_buffer_curr[off] = video_buffer_next[off];
                framebuffer[off] = video_buffer_next[off];
            }
            off++;
        }
        off += width - w;
    }
}

void fillrect(uint32_t color, int x, int y, int w, int h) {
    uint32_t* where = &video_buffer_next[x + y*width];
    int i, j;
 
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            *where++ = color;
        }
        where += width - w;
    }
}

void drawpic(uint32_t* buff, int x, int y, int w, int h) {
    if(x >= (int) width || y >= (int) height) {
        return;
    }
    if(x + w > (int) width) {
        w = width - x;
    }
    if(y + h > (int) height) {
        h = height - y;
    }
    uint32_t* where = &video_buffer_next[x + y*width];
    int i, j;    
 
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            *where++ = *buff++;
        }
        where += width - w;
    }
}
 
void drawchar(unsigned char c, int x, int y, uint32_t bgcolor, uint32_t fgcolor)
{
    int cy;
    int mask[8]={128,64,32,16,8,4,2,1}; // should match FONT_WIDTH
	unsigned char *glyph=font+(int)c*FONT_HEIGHT;
    uint32_t* buff = &video_buffer_next[x + y*width];
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
        buff += width - FONT_WIDTH;
	}
}

void screen_scroll_up(uint32_t row, uint32_t bgcolor)
{
    memmove(video_buffer_next, ((void*) video_buffer_next) + row * pitch, (height - row)*pitch);
    fillrect(bgcolor, 0, height - row, width, row);
}


void clear_screen(uint32_t bgcolor) {
    fillrect(bgcolor, 0, 0, width, height);
}

void get_screen_size(uint32_t* w, uint32_t* h)
{
    *w = width;
    *h = height;
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

    // we assume here and in tty that there is no padding between lines
    PANIC_ASSERT(pitch == bpp/8 * width);
    // drawchar uses this assumption
    PANIC_ASSERT(FONT_WIDTH == 8);

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

    // Use two video buffers, video_buffer_curr holds the current view and video_buffer_next holds the next view to be displayed/flushed to framebuffer
    video_buffer_curr = (uint32_t*) alloc_pages_consecutive_frames(curr_page_dir(), PAGE_COUNT_FROM_BYTES(width*height*bpp/8), true, NULL);
    memset(video_buffer_curr, 0, PAGE_COUNT_FROM_BYTES(width*height*bpp/8) * PAGE_SIZE);
    video_buffer_next = (uint32_t*) alloc_pages_consecutive_frames(curr_page_dir(), PAGE_COUNT_FROM_BYTES(width*height*bpp/8), true, NULL);
    memset(video_buffer_next, 0, PAGE_COUNT_FROM_BYTES(width*height*bpp/8) * PAGE_SIZE);
    // sync video buffer
    memmove(framebuffer, video_buffer_curr, buffer_byte_size);

    initialized = 1;


    // Video Driver Performance benchmark

    // uint64_t draw_time = 0;
    // uint64_t refresh_time = 0;
    
    // uint64_t t0 = rdtsc();

    // for(int i=0; i < 100; i++) {
    //     char c = (i%2 == 0)?'A':'B';
    //     uint64_t tt0 = rdtsc();
    //     for(int y=0;y < 37;y++) {
    //         for(int x=0;x < 100; x++) {
    //             drawchar(c, x * FONT_WIDTH, y * FONT_HEIGHT, 0x0, 0x00FFFFFF);
    //             // video_refresh_rect(x * FONT_WIDTH, y * FONT_HEIGHT, FONT_WIDTH, FONT_HEIGHT);
    //         }
    //     }
    //     uint64_t tt1 = rdtsc();

    //     video_refresh();
        
    //     uint64_t tt2 = rdtsc();
        
    //     draw_time += tt1 - tt0;
    //     refresh_time += tt2 - tt1;
    // }

    // uint64_t t1 = rdtsc();
    // uint64_t total_time = t1 - t0;
    // volatile uint64_t fps = 3600750000 / (total_time/100);

    // volatile uint64_t draw_frac = (draw_time * 100) / total_time;
    // volatile uint64_t refresh_frac = (refresh_time * 100) / total_time;
    // volatile uint64_t refresh_fps = 3600750000 / (refresh_time / 100);

    // Test drawpic
    
    // uint32_t* pic = (uint32_t*) alloc_pages_consecutive_frames(curr_page_dir(), PAGE_COUNT_FROM_BYTES(500*303*bpp/8), true, NULL);
    // for(int i=0; i<500*303; i++) {
    //     pic[i] = 0x000000FF;
    //     pic[i] += ((i*100 / 303) * 0xFF / 100) << 24;
    // }

    // drawpic(pic, 0, 0, 500, 303);
    // video_refresh();

    return 0;
}
