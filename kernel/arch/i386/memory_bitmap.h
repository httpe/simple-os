#ifndef _MEMORY_BITMAP_H
#define _MEMORY_BITMAP_H

#include <stdint.h>

// Macros used in the bitset algorithms.
// Get the 0-based frame index from 4KiB align address
#define FRAME_INDEX_FROM_ADDR(a) ((a) / 0x1000)
#define ADDR_FROM_FRAME_INDEX(a) ((a) * 0x1000)
// The bits are stored in an array of uint32_t
// so each uint32_t can store the status of 32 frames (4bytes * 8bits/byte)
#define ARRAY_INDEX_FROM_FRAME_INDEX(a) ((a) / (8 * 4))
// Get the 0-based offset into the uint32_t
#define BIT_OFFSET_FROM_FRAME_INDEX(a) ((a) % (8 * 4))
// Total number of frames: {4GB 32-bit addressable space: 2^32}/{4KiB: 2^12} = 2^20 = 0x100000
#define N_FRAMES 0x100000

void set_frame(uint32_t frame_idx);
void clear_frame(uint32_t frame_idx);
uint32_t test_frame(uint32_t frame_idx);
uint32_t first_free_frame();


#endif