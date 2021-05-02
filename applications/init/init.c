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

static void test_multi_process()
{
    printf("Test yielding\n");
    sys_yield();
    printf("Welcome Back User World!\n");

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
        printf("This is child, exiting with code 123\n");
        exit(123);
    }
}

static void test_libc() {
    printf("Welcome to %s!\n", "Newlib");
    printf("Current Epoch: %lld\n", time(NULL));

    const char* str2 = "Test malloc/free!\n";
    char* buf = malloc(100);
    memmove(buf, str2, strlen(str2)+1);
    printf("%s", buf);
    free(buf);
}

static void test_file_system() {
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

    // Test file creation and deletion
    fd = open("/home/newfile", O_CREAT|O_RDWR);
    close(fd);
    int res_link = link("/home/newfil", "/home/newfil.1");
    printf("Link(%d)\n", res_link);
    int res_rename = rename("/home/newfile", "/home/newfile.2");
    printf("Rename(%d)\n", res_rename);
    int res_unlink = unlink("/home/newfile.2");
    printf("Unlink(%d)\n", res_unlink);
    res_unlink = unlink("/home/newfile");
    printf("Unlink(%d)\n", res_unlink);
}


int main(int argc, char* argv[]) {
    (void) argc;
    (void) argv;

    // Matching STDIN_FILENO/STDOUT_FILENO/STDERR_FILENO in Newlib unistd.h
    // file descriptor 0: stdin, console, STDIN_FILENO
    open("/console", O_RDWR);
    // file descriptor 1: stdout, console, STDOUT_FILENO
    sys_dup(0);
    // file descriptor 2: stderr, console, STDERR_FILENO
    sys_dup(0);

    printf("Hello User World!\n");

    // Perform tests of user space features
    test_multi_process();
    test_libc();
    test_file_system();

    // Execute the shell
    char* shell_argv[] = {"/boot/usr/bin/shell.elf", NULL};
    printf("EXEC Shell\n");
    execve("/boot/usr/bin/shell.elf", shell_argv, NULL);
}