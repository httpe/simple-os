#include <stddef.h>
#include <syscall.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <common.h>
#include <fcntl.h>
#include <unistd.h>
#include <fs.h>
#include <sys/wait.h>

static inline _syscall0(SYS_YIELD, int, sys_yield)
static inline _syscall1(SYS_DUP, int, sys_dup, int, fd)

int main(int argc, char* argv[]) {
    (void) argc;
    (void) argv;

    int fd_stdin = open("/console", O_RDWR);
    int fd_stdout = sys_dup(0);
    int fd_stderr = sys_dup(0);
    UNUSED_ARG(fd_stderr);

    int ret;
    printf("Hello User World!\n");
    printf("Current Epoch: %lld\n", time(NULL));

    ret = sys_yield();
    printf("Welcome Back User World!\n");
    (void) ret;

    printf("Welcome to %s!\n", "libc");

    int fork_ret = fork();
    int child_exit_status;

    if(fork_ret) {
        // parent
        printf("This is parent, child PID: %u\n", fork_ret);
        // sys_yield();
        int wait_ret = wait(&child_exit_status);
        if(wait_ret < 0) {
            printf("No child exited\n");
        } else {
            printf("Child %u exited, exit code = %d\n", wait_ret, WEXITSTATUS(child_exit_status));
        }
    } else {
        // child
        char* argv[] = {"/boot/usr/bin/shell", NULL};
        printf("This is child, testing EXEC\n");
        execve("/boot/usr/bin/shell", argv, NULL);
    }

    const char* str2 = "Test malloc/free!\n";
    char* buf = malloc(100);
    memmove(buf, str2, strlen(str2)+1);
    printf("%s", buf);
    free(buf);

    struct stat st = {0};
    char buf1[100] = {0};
    int fd = open("/home/RAND.OM", O_RDWR);
    if(fd >= 0) {
        int read_in = read(fd, buf1, 10);
        fstat(fd, &st);
        int closed = close(fd);
        printf("FD(%d), READ(%d), CLOSE(%d), MODTIME(%lld)\n", fd, read_in, closed, st.st_mtim.tv_sec);
        printf("READ content: \n %s \n", buf1);
    } else {
        printf("OPEN error\n");
    }



    const char* to_write = "Hello User I/O World!";
    fd = open("/home/RAND.OM", O_RDWR);
    if(fd >= 0) {
        int written = write(fd, to_write, strlen(to_write) + 1);
        int lseek_res = lseek(fd, -(strlen(to_write) + 1), SEEK_CUR);
        memset(buf1, 0, 100);
        int read_in = read(fd, buf1, strlen(to_write) + 1);
        fstat(fd, &st);
        int closed = close(fd);
        printf("FD(%d), WRITE(%d), SEEK(%d), READ(%d), CLOSE(%d), MODTIME(%lld)\n", fd, written, lseek_res, read_in, closed, st.st_mtim.tv_sec);
        printf("READ content: \n %s \n", buf1);
    } else {
        printf("OPEN error\n");
    }

    char c;
    write(fd_stdin, "Input:\n", 7);
    while(1) {
        int read_in = read(fd_stdin, &c, 1);
        if(read_in == 1) {
            write(fd_stdout, &c, 1);
        }
    }
}