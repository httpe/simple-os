#ifndef _KERNEL_MEMORY_BITMAP_H
#define _KERNEL_MEMORY_BITMAP_H

#include <stdint.h>

// Macros used in the bitset algorithms.
// A frame is 4KiB in size
#define FRAME_SIZE 0x1000
// Total number of frames: {4GB 32-bit addressable space: 2^32}/{4KiB: 2^12} = 2^20 = 0x100000
#define N_FRAMES 0x100000
// Get the 0-based frame index from physical address, one frame is 4KiB in size
#define FRAME_INDEX_FROM_ADDR(a) ((a) / FRAME_SIZE)
// Get the 4KiB aligned physical address from frame index
#define ADDR_FROM_FRAME_INDEX(a) ((a) * FRAME_SIZE)
// The bits are stored in an array of uint32_t
// so each uint32_t can store the status of 32 frames (4bytes * 8bits/byte)
#define ARRAY_INDEX_FROM_FRAME_INDEX(a) ((a) / (8 * 4))
// Get the 0-based offset into the uint32_t
#define BIT_OFFSET_FROM_FRAME_INDEX(a) ((a) % (8 * 4))

void set_frame(uint32_t frame_idx);
void clear_frame(uint32_t frame_idx);
uint32_t test_frame(uint32_t frame_idx);
uint32_t first_free_frame();
void initialize_bitmap(uint32_t mbt_physical_addr);

#endif