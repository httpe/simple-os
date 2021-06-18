#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <kernel/syscall.h>

#define _syscall0(syscall_num, retval_type, name) \
retval_type name() \
{\
    int ret_code; \
    asm volatile ("push $0; int $88; pop %%ebx" \
    :"=a"(ret_code) \
    :"a"(syscall_num) \
    :"ebx"); \
    return (retval_type) ret_code; \
}

#define _syscall1(syscall_num, retval_type, name, argtype1, arg1) \
retval_type name(argtype1 arg1) \
{\
    int ret_code; \
    asm volatile ("push %2; push $0; int $88; pop %%ebx; pop %%ebx" \
    :"=a"(ret_code) \
    :"a"(syscall_num), "r"((unsigned int) arg1) \
    :"ebx"); \
    return (retval_type) ret_code; \
}

#define _syscall2(syscall_num, retval_type, name, argtype1, arg1, argtype2, arg2) \
retval_type name(argtype1 arg1, argtype2 arg2) \
{\
    int ret_code; \
    asm volatile ("push %2; push %3; push $0; int $88; pop %%ebx; pop %%ebx; pop %%ebx" \
    :"=a"(ret_code) \
    :"a"(syscall_num), "r"((unsigned int) arg2), "r"((unsigned int) arg1) \
    :"ebx"); \
    return (retval_type) ret_code; \
}

#define _syscall3(syscall_num, retval_type, name, argtype1, arg1, argtype2, arg2, argtype3, arg3) \
retval_type name(argtype1 arg1, argtype2 arg2, argtype3 arg3) \
{\
    int ret_code; \
    asm volatile ("push %2; push %3; push %4; push $0; int $88; pop %%ebx; pop %%ebx; pop %%ebx; pop %%ebx" \
    :"=a"(ret_code) \
    :"a"(syscall_num), "r"((unsigned int) arg3), "r"((unsigned int) arg2), "r"((unsigned int) arg1) \
    :"ebx"); \
    return (retval_type) ret_code; \
}

#define _syscall4(syscall_num, retval_type, name, argtype1, arg1, argtype2, arg2, argtype3, arg3, argtype4, arg4) \
retval_type name(argtype1 arg1, argtype2 arg2, argtype3 arg3, argtype4 arg4) \
{\
    int ret_code; \
    asm volatile ("push %2; push %3; push %4; push %5; push $0; int $88; pop %%ebx; pop %%ebx; pop %%ebx; pop %%ebx; pop %%ebx" \
    :"=a"(ret_code) \
    :"a"(syscall_num), "m"((unsigned int) arg4), "r"((unsigned int) arg3), "r"((unsigned int) arg2), "r"((unsigned int) arg1) \
    :"ebx"); \
    return (retval_type) ret_code; \
}

#define _syscall5(syscall_num, retval_type, name, argtype1, arg1, argtype2, arg2, argtype3, arg3, argtype4, arg4, argtype5, arg5) \
retval_type name(argtype1 arg1, argtype2 arg2, argtype3 arg3, argtype4 arg4, argtype5 arg5) \
{\
    int ret_code; \
    asm volatile ("push %2; push %3; push %4; push %5; push %6; push $0; int $88; pop %%ebx; pop %%ebx; pop %%ebx; pop %%ebx; pop %%ebx; pop %%ebx" \
    :"=a"(ret_code) \
    :"a"(syscall_num),  "m"((unsigned int) arg5), "m"((unsigned int) arg4), "r"((unsigned int) arg3), "r"((unsigned int) arg2), "r"((unsigned int) arg1) \
    :"ebx"); \
    return (retval_type) ret_code; \
}

#define _syscall6(syscall_num, retval_type, name, argtype1, arg1, argtype2, arg2, argtype3, arg3, argtype4, arg4, argtype5, arg5, argtype6, arg6) \
retval_type name(argtype1 arg1, argtype2 arg2, argtype3 arg3, argtype4 arg4, argtype5 arg5, argtype6 arg6) \
{\
    int ret_code; \
    asm volatile ("push %2; push %3; push %4; push %5; push %6; push %7; push $0; int $88; pop %%ebx; pop %%ebx; pop %%ebx; pop %%ebx; pop %%ebx; pop %%ebx; pop %%ebx" \
    :"=a"(ret_code) \
    :"a"(syscall_num), "m"((unsigned int) arg6), "m"((unsigned int) arg5), "m"((unsigned int) arg4), "r"((unsigned int) arg3), "r"((unsigned int) arg2), "r"((unsigned int) arg1) \
    :"ebx"); \
    return (retval_type) ret_code; \
}

#endif