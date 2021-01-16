#include "memory_bitmap.h"
#include <stdio.h>

// Memory bitmap

// A bitset of frames - used or free.
uint32_t frames[N_FRAMES];

// Static function to set a bit in the frames bitset
void set_frame(uint32_t frame_idx)
{
    // uint32_t frame_idx = FRAME_INDEX_FROM_ADDR(physical_addr);
    uint32_t idx = ARRAY_INDEX_FROM_FRAME_INDEX(frame_idx);
    uint32_t off = BIT_OFFSET_FROM_FRAME_INDEX(frame_idx);
    frames[idx] |= (0x1 << off);
}

// Static function to clear a bit in the frames bitset
void clear_frame(uint32_t frame_idx)
{
    // uint32_t frame_idx = FRAME_INDEX_FROM_ADDR(physical_addr);
    uint32_t idx = ARRAY_INDEX_FROM_FRAME_INDEX(frame_idx);
    uint32_t off = BIT_OFFSET_FROM_FRAME_INDEX(frame_idx);
    frames[idx] &= ~(0x1 << off);
}

// Static function to test if a bit is set.
uint32_t test_frame(uint32_t frame_idx)
{
    // uint32_t frame_idx = FRAME_INDEX_FROM_ADDR(physical_addr);
    uint32_t idx = ARRAY_INDEX_FROM_FRAME_INDEX(frame_idx);
    uint32_t off = BIT_OFFSET_FROM_FRAME_INDEX(frame_idx);
    return (frames[idx] & (0x1 << off));
}

// Static function to find the first free frame.
uint32_t first_free_frame()
{
    uint32_t i, j;
    for (i = 0; i < N_FRAMES; i++)
    {
        if (frames[i] != 0xFFFFFFFF) // nothing free, exit early.
        {
            // at least one bit is free here.
            for (j = 0; j < 32; j++)
            {
                uint32_t toTest = 0x1 << j;
                if (!(frames[i] & toTest))
                {
                    return i * 4 * 8 + j; // return the index of the first free frame
                }
            }
        }
    }

    printf("KERNEL PANIC: No free frame!\n");
    while (1);
}
