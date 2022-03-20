#ifndef _KERNEL_TYPES_H
#define _KERNEL_TYPES_H

#include <stdint.h>

#ifndef _SSIZE_T_DECLARED
typedef int32_t ssize_t;
#define	_SSIZE_T_DECLARED
#endif

#ifndef _SUSECONDS_T_DECLARED
typedef int32_t suseconds_t;
#define	_SUSECONDS_T_DECLARED
#endif

typedef __SIZE_TYPE__ size_t;
typedef int64_t time_t;

#define _SSIZE_T_DECLARED


#endif