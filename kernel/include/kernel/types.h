#ifndef _KERNEL_TYPES_H
#define _KERNEL_TYPES_H

#include <stdint.h>

// for newlib
#ifndef _SSIZE_T_DECLARED
// for SmallerC libc
#ifndef __SSIZE_T_DEF 
typedef int32_t ssize_t;
#define	_SSIZE_T_DECLARED
#define	__SSIZE_T_DEF
#endif
#endif

#ifndef _SUSECONDS_T_DECLARED
typedef int32_t suseconds_t;
#define	_SUSECONDS_T_DECLARED
#endif

#ifndef _SIZE_T_DECLARED
#ifndef __SIZE_T_DEF
typedef __SIZE_TYPE__ size_t;
#define	_SIZE_T_DECLARED
#define __SIZE_T_DEF
#endif
#endif

#ifndef _TIME_T_DECLARED
#ifndef __TIME_T_DEF
typedef int32_t time_t;
#define	_TIME_T_DECLARED
#define __TIME_T_DEF
#endif
#endif


#endif