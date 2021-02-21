#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <kernel/panic.h>
#include "paging.h"
#include <kernel/heap.h>


// Magic number for heap header on the left boundary of a (virtual address wise) contiguous space 
#define HEAP_HEADER_MAGIC_LEFT 0xBEAFFAEB
// Magic number for other heap header 
#define HEAP_HEADER_MAGIC_MID 0x11050522
// Magic number for heap footer on the right boundary of a (virtual address wise) contiguous space 
#define HEAP_FOOTER_MAGIC_RIGHT 0xBADADBAD
// Magic number for other heap footer
#define HEAP_FOOTER_MAGIC_MID 0x19310918

#define KERNEL_HEAP_INIT_SIZE_IN_PAGES 2
#define KERNEL_HEAP_MAX_SIZE_IN_PAGES 1024
#define HEAP_FOOTER_FROM_HEADER(header_ptr) (heap_footer_t*) ((uint32_t) (header_ptr) + sizeof(heap_header_t) + (header_ptr)->size)
#define ASSERT_VALID_HEAP_HEADER(header) PANIC_ASSERT((header)->magic == HEAP_HEADER_MAGIC_LEFT || (header)->magic == HEAP_HEADER_MAGIC_MID)
#define ASSERT_VALID_HEAP_FOOTER(footer) PANIC_ASSERT((footer)->magic == HEAP_FOOTER_MAGIC_MID || (footer)->magic == HEAP_FOOTER_MAGIC_RIGHT)

typedef struct heap_header {
    uint32_t magic;
    uint32_t size; // exclude header and footer
    struct heap_header* next; // if free, link to next block, if is last free, link to itself, otherwise set to NULL
    struct heap_header* prev; // if NULL, this header is the smallest header pointed by heap->smallest_free_header
} heap_header_t;

typedef struct heap_footer {
    uint32_t magic;
    struct heap_header* header;
} heap_footer_t;

typedef struct heap {
    struct heap_header* smallest_free_header;
    uint32_t size_in_pages; // total size counted in pages, including all meta data (header/footer and heap_t)
    uint32_t min_size_in_pages;
    uint32_t max_size_in_pages;
    bool is_kernel;
} heap_t;

static heap_t* kernel_heap;

heap_t* initialize_heap(uint32_t size_in_pages, uint32_t min_size_in_pages, uint32_t max_size_in_pages, bool is_kernel) {
    PANIC_ASSERT(size_in_pages > 0 && size_in_pages <= max_size_in_pages && size_in_pages >= min_size_in_pages);

    uint32_t heap_addr = alloc_pages(size_in_pages, is_kernel, true);
    heap_header_t* header = (heap_header_t*)(heap_addr + sizeof(heap_t));
    header->magic = HEAP_HEADER_MAGIC_MID; // MID because it is not started at page boundary 
    // header's size is usable size
    header->size = PAGE_SIZE * size_in_pages - sizeof(heap_t) - sizeof(heap_header_t) - sizeof(heap_footer_t);
    // Last block will have next pointing to itself
    header->next = header;
    header->prev = NULL;

    heap_footer_t* footer = HEAP_FOOTER_FROM_HEADER(header);
    footer->magic = HEAP_FOOTER_MAGIC_RIGHT;
    footer->header = header;

    heap_t* heap = (heap_t*)heap_addr;
    heap->smallest_free_header = header;
    heap->max_size_in_pages = max_size_in_pages;
    heap->min_size_in_pages = min_size_in_pages;
    heap->size_in_pages = size_in_pages;
    heap->is_kernel = is_kernel;
    return heap;
}

void initialize_kernel_heap() {
    kernel_heap = initialize_heap(KERNEL_HEAP_INIT_SIZE_IN_PAGES, KERNEL_HEAP_INIT_SIZE_IN_PAGES, KERNEL_HEAP_MAX_SIZE_IN_PAGES, true);
}

void insert_free_space(heap_t* heap, heap_header_t* free_header) {
    PANIC_ASSERT(free_header != NULL);
    ASSERT_VALID_HEAP_HEADER(free_header);
    heap_header_t* header = heap->smallest_free_header;
    if (!header) {
        // heap has no free space
        heap->smallest_free_header = free_header;
        // This header becomes the first block
        free_header->prev = NULL;
        free_header->next = free_header;
        return;
    }
    while (header->size < free_header->size)     {
        if (header->next == header) {
            // header is the last free block
            // so now make free_header as last free block
            header->next = free_header;
            free_header->next = free_header;
            free_header->prev = header;
            return;
        }
        header = header->next;
        ASSERT_VALID_HEAP_HEADER(header);
    }
    if (header == heap->smallest_free_header) {
        // free_header is the smallest
        free_header->next = header;
        header->prev = free_header;
        heap->smallest_free_header = free_header;
        free_header->prev = NULL;
        return;
    }
    // when size of free_header is between size of header->prev and header
    free_header->prev = header->prev;
    header->prev->next = free_header;
    header->prev = free_header;
    free_header->next = header;
    return;
}

void claim_free_space(heap_t* heap, heap_header_t* free_header) {
    PANIC_ASSERT(free_header != NULL);
    ASSERT_VALID_HEAP_HEADER(free_header);
    if (free_header->prev) {
        ASSERT_VALID_HEAP_HEADER(free_header->prev);
        PANIC_ASSERT(free_header->prev->next == free_header);
        PANIC_ASSERT(free_header->next != NULL); // make sure free_header it is free
        if (free_header->next == free_header) {
            // free_header is the last free block
            // now make free_header->prev the last block
            free_header->prev->next = free_header->prev;
        } else {
            // otherwise point free_header->prev's next to header's next and vice versa 
            // the block size order is maintained under this operation
            free_header->prev->next = free_header->next;
            free_header->next->prev = free_header->prev;
        }
    } else {
        // free_header is the first block in the heap
        if (free_header->next == free_header) {
            // free_header is also the last block in the heap
            // i.e. it is the only block
            heap->smallest_free_header = NULL;
        } else {
            heap->smallest_free_header = free_header->next;
            free_header->next->prev = NULL;
        }
    }
    // set next/prev to NULL for used block
    free_header->next = NULL;
    free_header->prev = NULL;
}

heap_header_t* unify_free_space(heap_t* heap, heap_header_t* free_header) {
    ASSERT_VALID_HEAP_HEADER(free_header);
    PANIC_ASSERT(free_header->next != NULL); // assert is a free block
    heap_footer_t* free_footer = HEAP_FOOTER_FROM_HEADER(free_header);

    heap_footer_t* left_footer = (heap_footer_t*)((uint32_t)free_header - sizeof(heap_footer_t));
    if (is_vaddr_accessible((uint32_t)left_footer, heap->is_kernel, true)) {
        if (left_footer->magic == HEAP_FOOTER_MAGIC_MID || left_footer->magic == HEAP_FOOTER_MAGIC_RIGHT) {
            heap_header_t* left_header = left_footer->header;
            ASSERT_VALID_HEAP_HEADER(left_header);
            if (left_header->next != NULL) {
                printf("Heap unify left\n");
                // if on the left there is a free block, merge free_header into it
                // [left_header | left_header->size | left_footer | free_header | free_header->size | free_footer]
                // =>
                // [left_header | left_header->size + sizeof(left_footer) + sizeof(free_header) + free_header->size | free_footer pointing to left_header]
                claim_free_space(heap, left_header);
                claim_free_space(heap, free_header);
                left_header->size += sizeof(heap_footer_t) + sizeof(heap_header_t) + free_header->size;
                memset(left_footer, 0, sizeof(heap_footer_t) + sizeof(heap_header_t));
                free_footer->header = left_header;
                insert_free_space(heap, left_header);
                return unify_free_space(heap, left_header);
            }
        }
    }
    heap_header_t* right_header = (heap_header_t*)((uint32_t)free_footer + sizeof(heap_footer_t));
    if (is_vaddr_accessible((uint32_t)right_header, heap->is_kernel, true)) {
        if ((right_header->magic == HEAP_HEADER_MAGIC_MID || right_header->magic == HEAP_HEADER_MAGIC_LEFT) && right_header->next != NULL) {
            printf("Heap unify right\n");
            // if on the right there is a free block, merge it into free_header
            // [free_header | free_header->size|free_footer|right_header|right_header->size|right_footer]
            // =>
            // [free_header | free_header->size + sizeof(free_footer) + sizeof(right_header) + right_header->size | right_footer pointing to free_header]
            heap_footer_t* right_footer = HEAP_FOOTER_FROM_HEADER(right_header);
            ASSERT_VALID_HEAP_FOOTER(right_footer);

            claim_free_space(heap, free_header);
            claim_free_space(heap, right_header);
            free_header->size += sizeof(heap_footer_t) + sizeof(heap_header_t) + right_header->size;
            memset(free_footer, 0, sizeof(heap_footer_t) + sizeof(heap_header_t));
            right_footer->header = free_header;
            insert_free_space(heap, free_header);
            return unify_free_space(heap, free_header);
        }
    }

    return free_header;
}

void heap_free(heap_t* heap, uint32_t vaddr) {
    heap_header_t* header = (heap_header_t*)(vaddr - sizeof(heap_header_t));
    ASSERT_VALID_HEAP_HEADER(header); // assert it is a heap managed space
    PANIC_ASSERT(header->next == NULL); // assert it is marked as used, not a free space
    insert_free_space(heap, header);
    heap_header_t* unified_free_header = unify_free_space(heap, header);
    heap_footer_t* unified_free_footer = HEAP_FOOTER_FROM_HEADER(unified_free_header);
    ASSERT_VALID_HEAP_HEADER(unified_free_header);
    ASSERT_VALID_HEAP_FOOTER(unified_free_footer);
    uint32_t total_size = sizeof(heap_header_t) + unified_free_header->size + sizeof(heap_footer_t);
    uint32_t page_count = total_size / PAGE_SIZE; // we need round down page count here, not PAGE_COUNT_FROM_BYTES, which is round up
    // contract heap size
    if (unified_free_header->magic == HEAP_HEADER_MAGIC_LEFT && unified_free_footer->magic == HEAP_FOOTER_MAGIC_RIGHT) {
        // if the unified free block is the whole block allocated when doing expansion:
        //   free it in whole to reverse the expansion
        if (heap->size_in_pages - page_count >= heap->min_size_in_pages) {
            printf("Contract heap: Reverse an expansion of %d pages\n", page_count);
            claim_free_space(heap, unified_free_header);
            memset(unified_free_header, 0, sizeof(heap_header_t));
            memset(unified_free_footer, 0, sizeof(heap_footer_t));
            dealloc_pages((uint32_t)unified_free_header, page_count);
            heap->size_in_pages -= page_count;
            return;
        }
    }
    if (unified_free_footer->magic == HEAP_FOOTER_MAGIC_RIGHT) {
        // if the unifed block is at the end of a contiguous virtual space
        uint32_t new_heap_size = heap->size_in_pages;
        while (new_heap_size >= heap->min_size_in_pages && heap->size_in_pages - new_heap_size <= page_count)         {
            new_heap_size = new_heap_size / 2;
        }
        new_heap_size = new_heap_size * 2;
        uint32_t page_to_shrink = heap->size_in_pages - new_heap_size;
        if (page_to_shrink > 0) {
            printf("Shrink heap: shrink %d pages\n", page_to_shrink);
            claim_free_space(heap, unified_free_header);
            memset(unified_free_footer, 0, sizeof(heap_footer_t));
            unified_free_header->size = unified_free_header->size - PAGE_SIZE * page_to_shrink;
            heap_footer_t* new_footer = HEAP_FOOTER_FROM_HEADER(unified_free_header);
            new_footer->magic = HEAP_FOOTER_MAGIC_RIGHT;
            new_footer->header = unified_free_header;
            insert_free_space(heap, unified_free_header);
            dealloc_pages((uint32_t)new_footer + sizeof(heap_footer_t), page_to_shrink);
            heap->size_in_pages -= page_to_shrink;
        }

    }

}

void kfree(uint32_t vaddr) {
    heap_free(kernel_heap, vaddr);
}


heap_header_t* expand_heap(heap_t* heap, heap_header_t* largest_free_header, size_t requested_size) {
    printf("Expand_heap: largetst block %d bytes, requested: %d bytes\n", largest_free_header->size, requested_size);

    PANIC_ASSERT(requested_size > 0);
    if (largest_free_header) {
        ASSERT_VALID_HEAP_HEADER(largest_free_header);
        PANIC_ASSERT(largest_free_header->size < requested_size);
    }

    uint32_t request_pages = PAGE_COUNT_FROM_BYTES(requested_size + sizeof(heap_header_t) + sizeof(heap_footer_t));
    uint32_t current_pages = heap->size_in_pages;
    uint32_t new_pages = current_pages * 2; //Doubling the heap size
    while (new_pages - current_pages < request_pages) {
        new_pages = new_pages * 2;
    }
    if (new_pages > heap->max_size_in_pages) {
        new_pages = heap->max_size_in_pages;
    }
    if (new_pages - current_pages < request_pages) {
        // expand failed, max limit too low
        return NULL;
    }
    request_pages = new_pages - current_pages;
    uint32_t new_block_addr = alloc_pages(request_pages, heap->is_kernel, true);
    if (new_block_addr == 0) {
        // allocation failed
        return NULL;
    }

    heap_header_t* header = (heap_header_t*)new_block_addr;
    header->magic = HEAP_HEADER_MAGIC_LEFT;
    header->size = PAGE_SIZE * request_pages - sizeof(heap_header_t) - sizeof(heap_footer_t);
    // this new block should be the largest in the free header list, otherwise expansion won't be triggered
    header->next = header;
    heap_footer_t* footer = HEAP_FOOTER_FROM_HEADER(header);
    footer->magic = HEAP_FOOTER_MAGIC_RIGHT;
    footer->header = header;
    if (largest_free_header) {
        largest_free_header->next = header;
        header->prev = largest_free_header;
    } else {
        heap->smallest_free_header = header;
        header->prev = NULL;
    }
    heap->size_in_pages = new_pages;

    return unify_free_space(heap, header);
}

heap_header_t* find_free_heap_node_fit_size(heap_t* heap, size_t size) {
    heap_header_t* header = heap->smallest_free_header;
    while (header) // if heap->smallest_free_header is NULL, skip
    {
        ASSERT_VALID_HEAP_HEADER(header);
        if (header->size >= size) {
            return header;
        }
        if (header->next == header) {
            // last free block will link to itself
            // now we know the largest block is still too small, thus break
            break;
        }
        header = header->next;
        PANIC_ASSERT(header != NULL); // free blocks should have non-null next
    }
    return header;
}

void* heap_alloc(heap_t* heap, size_t size) {
    heap_header_t* header = find_free_heap_node_fit_size(heap, size);
    if (header == NULL || header->size < size) {
        // No free space or free space too small: expand heap
        header = expand_heap(heap, header, size);
        if (header == NULL) {
            // allocation failed because of expansion failure 
            return 0;
        }
        ASSERT_VALID_HEAP_HEADER(header);
        PANIC_ASSERT(header->size >= size);
    }

    // remove the free space from the free space list
    claim_free_space(heap, header);

    if (header->size <= size + sizeof(heap_header_t) + sizeof(heap_footer_t)) {
        return (void*)header + sizeof(heap_header_t);
    }

    // If have space to store another set of header/footer, split
    // [header| header->size |footer] => [header (used)| size | new_footer to header | new_header | new free space | footer to new header ]

    heap_footer_t* footer = (heap_footer_t*)((uint32_t)header + sizeof(heap_header_t) + header->size);
    ASSERT_VALID_HEAP_FOOTER(footer);

    heap_header_t* new_header = (heap_header_t*)((uint32_t)header + sizeof(heap_header_t) + size + sizeof(heap_footer_t));
    new_header->magic = HEAP_HEADER_MAGIC_MID;
    new_header->size = (uint32_t)footer - (uint32_t)new_header - sizeof(heap_header_t);

    footer->header = new_header;

    heap_footer_t* new_footer = (heap_footer_t*)((uint32_t)new_header - sizeof(heap_footer_t));
    new_footer->magic = HEAP_FOOTER_MAGIC_MID;
    new_footer->header = header;

    header->size = size;

    insert_free_space(heap, new_header);

    return (void*)header + sizeof(heap_header_t);
}

void* kmalloc(size_t size) {
    return heap_alloc(kernel_heap, size);
}
