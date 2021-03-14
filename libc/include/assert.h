#ifndef _ASSERT_H
#define _ASSERT_H 1

#if defined(__is_libk)
#include <kernel/panic.h>
#define assert(condition) PANIC_ASSERT(condition)
#else
#define assert(condition) ((condition) ? (void)0 : user_panic(__FILE__, __LINE__, "ASSERTION-FAILED", #condition))
#endif

#endif