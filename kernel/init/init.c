#include <stddef.h>
#include <syscall.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int user_main(int argc, char* argv[]) {
    (void) argc;
    (void) argv;

    int ret = do_syscall_1(SYS_PRINT, "Hello User World!\n");
    ret = do_syscall_1(SYS_YIELD, NULL);
    ret = do_syscall_1(SYS_PRINT, "Welcome Back User World!\n");
    (void) ret;

    printf("Welcome to %s!\n", "libc");

    int fork_ret = do_syscall_1(SYS_FORK, NULL);
    for(int i=0; i<3; i++) {
        if(fork_ret) {
            // parent
            printf("This is parent, child PID: %u\n", fork_ret);
            // do_syscall_1(SYS_YIELD, NULL);
            do_syscall_1(SYS_WAIT, NULL);
        } else {
            // child
            printf("This is child\n");
            // do_syscall_1(SYS_YIELD, NULL);
        }
    }

    if(!fork_ret) {
        do_syscall_1(SYS_EXIT, 0);
    }
    
    ret = do_syscall_1(SYS_SBRK, (void*) 100);
    const char* str1 = "Test SBRK!\n";
    memmove((char*) ret, str1, strlen(str1)+1);
    printf("%s", (char*) ret);

    const char* str2 = "Test malloc/free!\n";
    char* buf = malloc(100);
    memmove(buf, str2, strlen(str2)+1);
    printf("%s", buf);
    free(buf);

    while(1);
}