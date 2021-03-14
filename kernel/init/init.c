#include <stddef.h>
#include <syscall.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <common.h>
#include <fs.h>

static inline _syscall0(SYS_YIELD, int, sys_yield)
static inline _syscall0(SYS_FORK, int, sys_fork)
static inline _syscall1(SYS_WAIT, int, sys_wait, int*, exit_code)
static inline _syscall1(SYS_SBRK, int, sys_sbrk, int, size_delta)
static inline _syscall2(SYS_OPEN, int, sys_open, char*, path, int, flags)
static inline _syscall1(SYS_CLOSE, int, sys_close, int, fd)
static inline _syscall3(SYS_READ, int, sys_read, int, fd, void*, buf, uint, size)
static inline _syscall3(SYS_WRITE, int, sys_write, int, fd, const void*, buf, uint, size)
static inline _syscall3(SYS_SEEK, int, sys_seek, int, fd, int, offset, int, whence)

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
            // sys_yield();
            int wait_ret = sys_wait(&child_exit_code);
            if(wait_ret < 0) {
                printf("No child exited\n");
            } else {
                printf("Child %u exited, exit code = %d\n", wait_ret, child_exit_code);
            }
        } else {
            // child
            printf("This is child\n");
            // sys_yield();
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

    char buf1[100] = {0};
    int fd = sys_open("/hdb/RAND.OM", 0);
    if(fd >= 0) {
        int read = sys_read(fd, buf1, 10);
        int close = sys_close(fd);
        printf("FD(%d), READ(%d), CLOSE(%d)\n", fd, read, close);
        printf("READ content: \n%s\n", buf1);
    } else {
        printf("OPEN error\n");
    }


    const char* to_write = "Hello User I/O World!";
    fd = sys_open("/hdb/RAND.OM", 0);
    if(fd >= 0) {
        int written = sys_write(fd, to_write, strlen(to_write) + 1);
        int lseek_res = sys_seek(fd, -(strlen(to_write) + 1), SEEK_WHENCE_CUR);
        memset(buf1, 0, 100);
        int read = sys_read(fd, buf1, strlen(to_write) + 1);
        int close = sys_close(fd);
        printf("FD(%d), WRITE(%d), LSEEK(%d), READ(%d), CLOSE(%d)\n", fd, written, lseek_res, read, close);
        printf("READ content: \n%s\n", buf1);
    } else {
        printf("OPEN error\n");
    }


    while(1);
}