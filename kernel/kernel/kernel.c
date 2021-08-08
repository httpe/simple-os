#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common.h>
#include <kernel/time.h>
#include <kernel/tty.h>
#include <kernel/arch_init.h>
#include <kernel/heap.h>
#include <kernel/ata.h>
#include <kernel/panic.h>
#include <kernel/process.h>
#include <kernel/block_io.h>
#include <kernel/vfs.h>
#include <kernel/network.h>
#include <kernel/video.h>
#include <arch/i386/kernel/cpu.h>

typedef void entry_main(void);

void test_malloc()
{
	char* a = kmalloc(8);
	printf("Heap alloc a[8]: %u\n", a);
	char* b = kmalloc(8);
	printf("Heap alloc b[8]: %u\n", b);
	char* c = kmalloc(8);
	printf("Heap alloc c[8]: %u\n", c);
	printf("free(a):\n", c);
	kfree(a);
	printf("free(c):\n", c);
	kfree(c);
	printf("free(b):\n", c);
	kfree(b);
	char* d = kmalloc(4096 * 3);
	printf("Heap alloc d[4096*3]: %u\n", d);
	char* e = kmalloc(8);
	printf("Heap alloc e[8]: %u\n", e);
	printf("free(d)\n");
	kfree(d);
	printf("free(e)\n");
	kfree(e);

	// Kernel Heap Performance Benchmark
	uint64_t t0 = rdtsc();
    void* buf[100];
    for(int i=0; i<10; i++) {
        for(int j=0;j<100;j++) {
            buf[j] = kmalloc(1234);
        }
        for(int j=0;j<100;j++) {
            kfree(buf[j]);
        }
    }
    uint64_t t1 = rdtsc();
    int64_t total_cycle = t1 - t0;
	int64_t freq = cpu_freq();
    int64_t op_per_sec = freq / (total_cycle / (10*100));
	printf("Kernel heap benchmark: %lld operations per second\n", op_per_sec);
}

void test_ata()
{
	char* mbr = kmalloc(512);
	int32_t max_lba = get_total_28bit_sectors(false);
	printf("Disk max 28bit addressable LBA: %d\n", max_lba);
    read_sectors_ATA_PIO(false, mbr, 0, 1);

	printf("(ATA) Disk MBR last four bytes: 0x%x\n", *(uint32_t*) &mbr[508]);

	memset(mbr, 0, 512);
	
	block_storage* storage = get_block_storage(2);
	if(storage != NULL) {
		storage->read_blocks(storage, mbr, 0, 1);
		printf("(Block IO slave) Disk MBR last four bytes: 0x%x\n", *(uint32_t*) &mbr[508]);
	}
	int i = 0;
	fs_dirent* dirent_buf = malloc(sizeof(fs_dirent));
	while(fs_readdir("/", i++, dirent_buf, sizeof(fs_dirent)) > 0) {
		printf("Enum root dir: %s\n", dirent_buf->name);
	}
	free(dirent_buf);

	kfree(mbr);

}

void test_paging()
{
	// Manually triggering a page fault
	// Use volatile to avoid it get optimized away
	printf("Manually triggering a page fault at 0xA0000000...\n");
	uint32_t* ptr = (uint32_t*)0xA0000000;
	uint32_t volatile do_page_fault = *ptr;
	UNUSED_ARG(do_page_fault);
	PANIC("Failed to trigger page fault");
}

uint test_video()
{
    // Run Video Driver Performance benchmark
    uint64_t draw_time = 0;
    uint64_t refresh_time = 0;
    
    const uint n_iteration = 100;

    uint64_t t0 = rdtsc();

	uint32_t font_width, font_height, screen_width, screen_height;
	get_vga_font_size(&font_width, &font_height);
	get_screen_size(&screen_width, &screen_height);
	uint char_w = screen_width/font_width, char_h = screen_height/font_height;

    for(uint i=0; i < n_iteration; i++) {
        char c = (i%2 == 0)?'A':'B';

        uint64_t tt0 = rdtsc();
        for(uint y=0;y < char_h;y++) {
            for(uint x=0;x < char_w; x++) {
                drawchar(c, x * FONT_WIDTH, y * FONT_HEIGHT, 0x0, 0x00FFFFFF);
                // video_refresh_rect(x * FONT_WIDTH, y * FONT_HEIGHT, FONT_WIDTH, FONT_HEIGHT);
            }
        }
        uint64_t tt1 = rdtsc();
        video_refresh();
        uint64_t tt2 = rdtsc();

        draw_time += tt1 - tt0;
        refresh_time += tt2 - tt1;
    }

    uint64_t t1 = rdtsc();
    uint64_t total_cycle = t1 - t0;
    int64_t freq = cpu_freq();
    uint fps = freq / (total_cycle / n_iteration);

    uint draw_pct = (draw_time * 100) / total_cycle;
    uint refresh_pct = (refresh_time * 100) / total_cycle;
    uint refresh_fps = freq / (refresh_time / n_iteration);

    printf("Video Benchmark: FPS[%d]:RefreshFPS[%d]:DRAW_PCT[%d]:REFRESH_PCT[%d]\n", fps, refresh_fps, draw_pct, refresh_pct);
    
    return fps;
}

void init()
{
	initialize_block_storage();
	init_vfs();
	init_network();
}

void kernel_main(uint32_t mbt_physical_addr) {

	// Architecture specific initialization
	initialize_architecture(mbt_physical_addr);
	
	// Non-architecture specific initialization
	init();
	
	terminal_set_font_attr(TTY_FONT_ATTR_BLINK);
	printf("Welcome to Simple-OS!\n");
	date_time dt = current_datetime();
	printf("CMOS Date Time: %u-%u-%u %u:%u:%u\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
	terminal_set_font_attr(TTY_FONT_ATTR_CLEAR);

	// Kernel tests
	// test_malloc();
	// test_ata();
	// test_paging();
	// test_video();

	// unused tests
	UNUSED_ARG(test_malloc);
	UNUSED_ARG(test_ata);
	UNUSED_ARG(test_paging);
	UNUSED_ARG(test_video);

	// Enter user space and running init
	init_first_process();
	scheduler();

	PANIC("Returned from scheduler");
}

