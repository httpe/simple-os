#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syscall.h>
#include <fs.h>
#include <kernel/keyboard.h>

#define FD_STDIN 0
#define FD_STDOUT 1
#define FD_STDERR 2

#define MAX_COMMAND_LEN 255

static inline _syscall4(SYS_READDIR, int, sys_readdir, const char *, path, uint, entry_offset, fs_dirent*, buf, uint, buf_size)

int main(int argc, char* argv[]) {
    printf("Welcome to the Shell!\n");
    printf("Shell ARGC(%d)\n", argc);
    for(int i=0; i<argc; i++) {
        printf("  %d: %s\n", i, argv[i]);
    }
    printf("Use 'help' command to show usage\n");
   
    char c;
    char command[MAX_COMMAND_LEN + 1] = {0};
    int n_command_read;

    while(1) {
        write(FD_STDOUT, "$ ", 2);
        memset(command, 0, MAX_COMMAND_LEN + 1);
        n_command_read = 0;
        while(1) {
            while(read(FD_STDIN, &c, 1) <= 0);
            if(c == '\n' || n_command_read >= MAX_COMMAND_LEN) {
                write(FD_STDOUT, &c, 1);
                break;
            }
            if(c == '\b') {
                if(n_command_read > 0) {
                    write(FD_STDOUT, &c, 1);
                    n_command_read--;
                }
            } else {
                write(FD_STDOUT, &c, 1);
                command[n_command_read++] = c;
            }
        }

        char* part = strtok(command," ");
        if(part == NULL) {
            continue;
        }
        if(strcmp(part, "help") == 0) {
            printf("Supported commands\n");
            printf("ls: listing dir\n");
        } else if(strcmp(part, "ls") == 0) {
            fs_dirent entry = {0};
            int i = 0;
            char* ls_path = strtok(NULL," ");
            if(ls_path == NULL) {
                ls_path = "/";
            }
            while(sys_readdir(ls_path, i++, &entry, sizeof(fs_dirent)) > 0) {
                printf("  File: %s\n", entry.name);
            }
        }
        else {
            printf("Unknow command:\n%s\n", command);
        }

    }


    return 0;
}