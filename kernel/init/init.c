#define SYS_PRINT 2

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
    int ret = do_sys_call(SYS_PRINT, "Hello World!");

    while(1);
}