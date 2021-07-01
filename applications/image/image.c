#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <syscall.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS
#include "stb_image.h"

static inline _syscall5(SYS_DRAW_PICTURE, int, draw_picture, uint32_t*, buff, int, x, int, y, int, w, int, h)
static inline _syscall0(SYS_REFRESH_SCREEN, int, sys_refresh_screen)

int main(int argc, char* argv[]) {

    // uint32_t* buff = (uint32_t*) malloc(500*303*sizeof(uint32_t));
    // memset(buff, 0, 500*303*sizeof(uint32_t));
    // for(int i=0; i<500*303; i++) {
    //     buff[i] = 0x0066CCFF;
    // }
    // draw_picture(buff, 0, 0, 500, 303);
    // sys_refresh_screen();


    if(argc !=2 || argv[1] == NULL) {
        printf("Usage: image <path to image file>\n");
        exit(1);
    }

    //    // ... process data if not NULL ...
    //    // ... x = width, y = height, n = # 8-bit components per pixel ...
    //    // ... replace '0' with '1'..'4' to force that many components per pixel
    //    // ... but 'n' will always be the number that it would have been if you said 0
    //
    // Standard parameters:
    //    int *x                 -- outputs image width in pixels
    //    int *y                 -- outputs image height in pixels
    //    int *channels_in_file  -- outputs # of image components in image file
    //    int desired_channels   -- if non-zero, # of image components requested in result
    int x,y,n;
    uint32_t *data = (uint32_t*) stbi_load(argv[1], &x, &y, &n, 4);

    for(int i=0; i<x*y; i++) {
        // uint32_t a =  (data[i] >> 24) & 0xFF;
        uint32_t b =  (data[i] >> 16) & 0xFF;
        uint32_t g =  (data[i] >> 8) & 0xFF;
        uint32_t r =  (data[i]) & 0xFF;
        data[i] = (r << 16) | (g << 8) | (b << 0);
    }

    draw_picture(data, 0, 0, x, y);
    sys_refresh_screen();

    stbi_image_free(data);

    // wait for the user to press any key
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1);

    // Clear screen before exit
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    exit(0);
}