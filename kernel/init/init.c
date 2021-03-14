#include <stddef.h>
#include <syscall.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static inline _syscall0(SYS_YIELD, int, sys_yield)
static inline _syscall0(SYS_FORK, int, sys_fork)
static inline _syscall1(SYS_WAIT, int, sys_wait, int*, exit_code)
static inline _syscall1(SYS_SBRK, int, sys_sbrk, int, size_delta)

int user_main(int argc, char* argv[]) {
    (void) argc;
    (void) argv;

    int ret = printf("Hello User World!\n");
    ret = sys_yield();
    ret = printf("Welcome Back User World!\n");
    (void) ret;

    printf("Welcome to %s!\n", "libc");

    int fork_ret = sys_fork();
    int child_exit_code;
    for(int i=0; i<3; i++) {
        if(fork_ret) {
            // parent
            printf("This is parent, child PID: %u\n", fork_ret);
            // do_syscall_1(SYS_YIELD, NULL);
            int wait_ret = sys_wait(&child_exit_code);
            if(wait_ret < 0) {
                printf("No child exited\n");
            } else {
                printf("Child %u exited, exit code = %d\n", wait_ret, child_exit_code);
            }
        } else {
            // child
            printf("This is child\n");
            // do_syscall_1(SYS_YIELD, NULL);
        }
    }

    if(!fork_ret) {
        // exit child process
        abort();
    }
    
    ret = sys_sbrk(100);
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