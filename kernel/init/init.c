#include <stddef.h>
#include <syscall.h>

int do_sys_call(int sys_call_num, void* arg)
{
    int ret_code;
    asm volatile ("push %2; push $0; int $88; pop %%ebx; pop %%ebx"
    :"=a"(ret_code)
    :"a"(sys_call_num), "r"((unsigned int) arg)
    :"ebx");
    return ret_code;
};

int user_main(int argc, char* argv[]) {
    (void) argc;
    (void) argv;

    int ret = do_sys_call(SYS_PRINT, "Hello User World!");
    ret = do_sys_call(SYS_YIELD, NULL);
    ret = do_sys_call(SYS_PRINT, "Welcome Back User World!");
    (void) ret;

    int fork_ret = do_sys_call(SYS_FORK, NULL);
    for(int i=0; i<3; i++) {
        if(fork_ret) {
            // parent
            do_sys_call(SYS_PRINT, "This is parent!");
            do_sys_call(SYS_YIELD, NULL);
        } else {
            // child
            do_sys_call(SYS_PRINT, "This is child!");
            do_sys_call(SYS_YIELD, NULL);
        }
    }
    
    while(1);
}