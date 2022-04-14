#include <string.h>

// Copied from newlib/libc/string/memmove.c

/* Nonzero if either X or Y is not aligned on a "long" boundary.  */
#define UNALIGNED(X, Y) \
  (((long)X & (sizeof (long) - 1)) | ((long)Y & (sizeof (long) - 1)))

/* How many bytes are copied each iteration of the 4X unrolled loop.  */
#define BIGBLOCKSIZE    (sizeof (long) << 2)

/* How many bytes are copied each iteration of the word copy loop.  */
#define LITTLEBLOCKSIZE (sizeof (long))

/* Threshhold for punting to the byte copier.  */
#define TOO_SMALL(LEN)  ((LEN) < BIGBLOCKSIZE)


// Original naive memmove implementation
// void* memmove(void* dstptr, const void* srcptr, size_t size) {
// 	unsigned char* dst = (unsigned char*) dstptr;
// 	const unsigned char* src = (const unsigned char*) srcptr;
// 	if (dst < src) {
// 		for (size_t i = 0; i < size; i++)
// 			dst[i] = src[i];
// 	} else {
// 		for (size_t i = size; i != 0; i--)
// 			dst[i-1] = src[i-1];
// 	}
// 	return dstptr;
// }

// Optimized memcpy from Newlib
void* memmove(void* dst_void, const void* src_void, size_t length) {
	if(dst_void == src_void) return dst_void;

	char* dst = dst_void;
	const char* src = src_void;
	long* aligned_dst;
	const long* aligned_src;

	if (src < dst && dst < src + length) {
		/* Destructive overlap...have to copy backwards */
		src += length;
		dst += length;
		while (length--) {
			*--dst = *--src;
		}
	} else {
		/* Use optimizing algorithm for a non-destructive copy to closely
		   match memcpy. If the size is small or either SRC or DST is unaligned,
		   then punt into the byte copy loop.  This should be rare.  */
		if (!TOO_SMALL(length) && !UNALIGNED(src, dst)) {
			aligned_dst = (long*)dst;
			aligned_src = (long*)src;

			/* Copy 4X long words at a time if possible.  */
			while (length >= BIGBLOCKSIZE) {
				*aligned_dst++ = *aligned_src++;
				*aligned_dst++ = *aligned_src++;
				*aligned_dst++ = *aligned_src++;
				*aligned_dst++ = *aligned_src++;
				length -= BIGBLOCKSIZE;
			}

			/* Copy one long word at a time if possible.  */
			while (length >= LITTLEBLOCKSIZE) {
				*aligned_dst++ = *aligned_src++;
				length -= LITTLEBLOCKSIZE;
			}

			/* Pick up any residual with a byte copier.  */
			dst = (char*)aligned_dst;
			src = (char*)aligned_src;
		}

		while (length--) {
			*dst++ = *src++;
		}
	}

	return dst_void;
}
