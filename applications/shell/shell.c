#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syscall.h>
#include <fs.h>
#include <kernel/keyboard.h>

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
    char prev_command[MAX_COMMAND_LEN + 1] = {0};
    int prev_n_command_read;

    while(1) {
        write(STDOUT_FILENO, "$ ", 2);
        memset(command, 0, MAX_COMMAND_LEN + 1);
        n_command_read = 0;
        while(1) {
            while(read(STDIN_FILENO, &c, 1) <= 0);
            if(c == '\n') {
                write(STDOUT_FILENO, &c, 1);
                break;
            } else if(c == '\b') {
                if(n_command_read > 0) {
                    write(STDOUT_FILENO, &c, 1);
                    n_command_read--;
                }
            } else if(c == KEY_UP) {
                // clear command entered
                for(int i=0; i<n_command_read;i++) {
                    write(STDOUT_FILENO, &"\b", 1);
                }
                memmove(command, prev_command, MAX_COMMAND_LEN+1);
                n_command_read = prev_n_command_read;
                write(STDOUT_FILENO, command, n_command_read);
            } else if(n_command_read < MAX_COMMAND_LEN) {
                write(STDOUT_FILENO, &c, 1);
                command[n_command_read++] = c;
            }
        }
        memmove(prev_command, command, MAX_COMMAND_LEN + 1);
        prev_n_command_read = n_command_read;

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