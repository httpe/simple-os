#include <stddef.h>
#include <syscall.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <common.h>
#include <fcntl.h>
#include <unistd.h>
#include <fs.h>

static inline _syscall0(SYS_YIELD, int, sys_yield)
// static inline _syscall0(SYS_FORK, int, sys_fork)
static inline _syscall1(SYS_WAIT, int, sys_wait, int*, exit_code)
// static inline _syscall1(SYS_SBRK, int, sys_sbrk, int, size_delta)
// static inline _syscall2(SYS_OPEN, int, sys_open, char*, path, int, flags)
// static inline _syscall1(SYS_CLOSE, int, sys_close, int, fd)
// static inline _syscall3(SYS_READ, int, sys_read, int, fd, void*, buf, uint, size)
// static inline _syscall3(SYS_WRITE, int, sys_write, int, fd, const void*, buf, uint, size)
static inline _syscall3(SYS_SEEK, int, sys_seek, int, fd, int, offset, int, whence)

int main(int argc, char* argv[]) {
    (void) argc;
    (void) argv;

    int fd_stdin = open("/console", 0);
    if(fd_stdin < 0) {
        printf("OPEN error\n");
    }
    int fd_stdout = open("/console", 0);
    if(fd_stdout < 0) {
        printf("OPEN error\n");
    }

    int ret = printf("Hello User World!\n");
    ret = sys_yield();
    ret = printf("Welcome Back User World!\n");
    (void) ret;

    printf("Welcome to %s!\n", "libc");

    int fork_ret = fork();
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
        exit(0);
    }
    
    // ret = sys_sbrk(100);
    // const char* str1 = "Test SBRK!\n";
    // memmove((char*) ret, str1, strlen(str1)+1);
    // printf("%s", (char*) ret);

    const char* str2 = "Test malloc/free!\n";
    char* buf = malloc(100);
    memmove(buf, str2, strlen(str2)+1);
    printf("%s", buf);
    free(buf);

    // char buf1[100] = {0};
    // int fd = open("/home/RAND.OM", 0);
    // if(fd >= 0) {
    //     int read_in = read(fd, buf1, 10);
    //     int closed = close(fd);
    //     printf("FD(%d), READ(%d), CLOSE(%d)\n", fd, read_in, closed);
    //     printf("READ content: \n %s \n", buf1);
    // } else {
    //     printf("OPEN error\n");
    // }


    // const char* to_write = "Hello User I/O World!";
    // fd = open("/home/RAND.OM", 0);
    // if(fd >= 0) {
    //     int written = write(fd, to_write, strlen(to_write) + 1);
    //     int lseek_res = sys_seek(fd, -(strlen(to_write) + 1), SEEK_WHENCE_CUR);
    //     memset(buf1, 0, 100);
    //     int read_in = read(fd, buf1, strlen(to_write) + 1);
    //     int closed = close(fd);
    //     printf("FD(%d), WRITE(%d), SEEK(%d), READ(%d), CLOSE(%d)\n", fd, written, lseek_res, read_in, closed);
    //     printf("READ content: \n %s \n", buf1);
    // } else {
    //     printf("OPEN error\n");
    // }

    char c;
    write(fd_stdin, "Input:\n", 7);
    while(1) {
        int read_in = read(fd_stdin, &c, 1);
        if(read_in == 1) {
            write(fd_stdout, &c, 1);
        }
    }
}