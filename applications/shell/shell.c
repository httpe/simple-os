#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syscall.h>
#include <stdlib.h>
#include <fcntl.h>
#include <fs.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_COMMAND_LEN 255
#define MAX_PATH_LEN 4096
#define MAX_ARGC 10

static inline _syscall4(SYS_READDIR, int, sys_readdir, const char *, path, uint, entry_offset, fs_dirent*, buf, uint, buf_size)

enum specialKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';
  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
  return 0;
}

int readKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) return 0;
  }

  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }

    return '\x1b';
  } else {
    return c;
  }
}

int main(int argc, char* argv[]) {
    // Clear screen
    write(STDOUT_FILENO, "\x1b[2J", 4);

    // Get Terminal Width/Height by first move cursor to the bottom right
    //  and then read the cursor position 
    write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12);
    int row, col;
    getCursorPosition(&row, &col);

    // Move cursor to top left
    write(STDOUT_FILENO, "\x1b[H", 3);

    printf("Welcome to the Shell (%d x %d)!\n", row, col);
    printf("Shell ARGC(%d)\n", argc);
    for(int i=0; i<argc; i++) {
        printf("  %d: %s\n", i, argv[i]);
    }
    printf("Use 'help' command to show usage\n");
    
    
    char command[MAX_COMMAND_LEN + 1] = {0};
    int n_command_read;
    char prev_command[MAX_COMMAND_LEN + 1] = {0};
    int prev_n_command_read;

    char* cwd = malloc(MAX_PATH_LEN+1);
    char* path_buff = malloc(MAX_PATH_LEN+1);

    while(1) {
        char* r_cwd = getcwd(cwd, MAX_PATH_LEN+1);
        if(r_cwd == NULL) {
            write(STDOUT_FILENO, "...", 3);
        } else {
            write(STDOUT_FILENO, cwd, strlen(cwd));
        }
        write(STDOUT_FILENO, "$ ", 2);

        memset(command, 0, MAX_COMMAND_LEN + 1);
        n_command_read = 0;
        while(1) {
            int k;
            while((k = readKey()) <= 0);
            if(k == '\n') {
                write(STDOUT_FILENO, &"\n", 1);
                break;
            } else if(k == '\x7F' || k == '\b') {
                if(n_command_read > 0) {
                    write(STDOUT_FILENO, "\b", 1);
                    command[--n_command_read] = 0;
                }
            } else if(k == ARROW_UP) {
                // clear command entered
                for(int i=0; i<n_command_read;i++) {
                    write(STDOUT_FILENO, &"\b", 1);
                }
                memmove(command, prev_command, MAX_COMMAND_LEN+1);
                n_command_read = prev_n_command_read;
                write(STDOUT_FILENO, command, n_command_read);
            } else if(n_command_read < MAX_COMMAND_LEN && k >= ' ' && k <= '~') {
                char c = (char) k;
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
            printf("Supported commands:\n");
            printf("ls: listing dir\n");
            printf("cd: changing current dir\n");
            printf("{program name}: Run program named {program name}.elf. Seach in various paths\n");
        } else if(strcmp(part, "ls") == 0) {
            fs_dirent entry = {0};
            char* ls_path = strtok(NULL," ");
            if(ls_path == NULL) {
                ls_path = "";
            }
            int i = 0;
            while(1) {
                int r = sys_readdir(ls_path, i++, &entry, sizeof(fs_dirent));
                if(r < 0) {
                    printf("ls error(%d): %s\n", r, strerror(-r));
                }
                if(r <= 0) {
                    break;
                }
                size_t path_len = strlen(ls_path);
                size_t name_len = strlen(entry.name);
                if(path_len + 1 + name_len > MAX_PATH_LEN) {
                    printf("  File: %s\n", entry.name);
                } else {
                    memmove(path_buff, ls_path, path_len);
                    if(path_len > 0 && ls_path[path_len-1] != '/') {
                        path_buff[path_len++] = '/';
                    }
                    memmove(path_buff + path_len, entry.name, name_len);
                    path_buff[path_len + name_len] = 0;
                    struct stat st = {0};
                    int r_stat = stat(path_buff, &st);
                    if(r_stat < 0) {
                        printf("ls %s stat error(%d): %s\n", entry.name, r_stat, strerror(-r_stat));
                    } else {
                        char* type;
                        if(S_ISDIR(st.st_mode)) {
                            type = "DIR";
                        } else {
                            type = "FILE";
                        }
                        char* datetime = ctime(&st.st_mtim.tv_sec);
                        // ctime result includes a trailing '\n', remove it
                        char buf[64] = {0};
                        memmove(buf, datetime, strlen(datetime)-1);
                        printf("  %-4s %s %10ld: %s\n", type, buf, st.st_size, entry.name);
                    }
                }
            }
        } else if(strcmp(part, "cd") == 0) {
            char* cd_path = strtok(NULL," ");
            if(cd_path == NULL) {
                continue;
            }
            int r = chdir(cd_path);
            if(r < 0) {
                printf("cd error(%d): %s\n", r, strerror(-r));
            }
        } else {
            // try execute programs
            struct stat st = {0};
            char prog_path[255] = {0};
            if(*part == '/') {
              strncpy(prog_path, part, 254);
            } else {
              // Mimic PATH environ vairbale
              // Search for these places for the binary, also append the .elf suffix
              char* PATH[]= {"/usr/bin/","/home/bin/","", NULL};
              char** path_prefix = PATH;
              for(;*path_prefix; path_prefix++) {
                int prefix_len = strlen(*path_prefix);
                int name_len = strlen(part);
                strncpy(prog_path, *path_prefix, 250);
                strncat(prog_path, part, 250 - prefix_len);
                strncat(prog_path, ".elf", 250 - prefix_len - name_len);
                int r_stat = stat(prog_path, &st);
                if(r_stat == 0 && S_ISREG(st.st_mode)) {
                  break;
                }
              }
              if(*path_prefix == NULL) {
                printf("shell exec: Executable ELF not found\n");
                continue;
              }
            }

            char* program_argv[MAX_ARGC + 1] = {NULL};
            program_argv[0] = prog_path;
            for(int i=1;i<MAX_ARGC;i++) {
              char* arg = strtok(NULL," ");
              if(arg == NULL) {
                break;
              }
              program_argv[i] = arg;
            }

            int fork_ret = fork();
            int child_exit_status;

            if(fork_ret) {
                // parent
                while(1) {
                  int wait_ret = wait(&child_exit_status);
                  if(wait_ret < 0) {
                      // printf("No child exited\n");
                  } else {
                      printf("Child %u exited, exit code = %d\n", wait_ret, WEXITSTATUS(child_exit_status));
                      break;
                  }
                }
            } else {
                // child
                int ret = execve(program_argv[0], program_argv, NULL);
                printf("shell exec error(%d)\n", ret);
                exit(ret);
            }
            
        }

    }


    return 0;
}