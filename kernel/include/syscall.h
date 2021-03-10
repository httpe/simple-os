#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <kernel/syscall.h>

static inline int do_syscall_1(int sys_call_num, void* arg)
{
    int ret_code;
    asm volatile ("push %2; push $0; int $88; pop %%ebx; pop %%ebx"
    :"=a"(ret_code)
    :"a"(sys_call_num), "r"((unsigned int) arg)
    :"ebx");
    return ret_code;
};

static inline int do_syscall_2(int sys_call_num, void* arg1, void* arg2)
{
    int ret_code;
    asm volatile ("push %2; push %3; push $0; int $88; pop %%ebx; pop %%ebx; pop %%ebx"
    :"=a"(ret_code)
    :"a"(sys_call_num), "r"((unsigned int) arg1), "r"((unsigned int) arg2)
    :"ebx");
    return ret_code;
};

static inline int do_syscall_3(int sys_call_num,  void* arg1, void* arg2, void* arg3)
{
    int ret_code;
    asm volatile ("push %2; push %3; push %4; push $0; int $88; pop %%ebx; pop %%ebx; pop %%ebx; pop %%ebx"
    :"=a"(ret_code)
    :"a"(sys_call_num), "r"((unsigned int) arg1), "r"((unsigned int) arg2), "r"((unsigned int) arg3)
    :"ebx");
    return ret_code;
};

#endif