#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <fcntl.h>
#include <fs.h>
#include <errno.h>
#include <common.h>
#include <dirent.h>

#define MAX_PATH_LEN 4096

int main(int argc, char* argv[]) {
    UNUSED_ARG(argc);

    char* ls_path = argv[1];

    char* path_buff = malloc(MAX_PATH_LEN+1);

    if(ls_path == NULL) {
        ls_path = "";
    }
    
    DIR* dir = opendir(ls_path);
    if(dir == NULL) {
        perror("ls error");
        exit(1);
    }
    struct dirent* entry;
    while(1) {
        entry = readdir(dir);
        if(entry == NULL) {
            if(errno != 0) {
                perror("ls error");
                closedir(dir);
                exit(1);
            } else {
                closedir(dir);
                exit(0);
            }
        }
        
        size_t path_len = strlen(ls_path);
        size_t name_len = strlen(entry->d_name);
        if(path_len + 1 + name_len > MAX_PATH_LEN) {
            printf("  File: %s\n", entry->d_name);
        } else {
            memmove(path_buff, ls_path, path_len);
            if(path_len > 0 && ls_path[path_len-1] != '/') {
                path_buff[path_len++] = '/';
            }
            memmove(path_buff + path_len, entry->d_name, name_len);
            path_buff[path_len + name_len] = 0;
            struct stat st = {0};
            int r_stat = stat(path_buff, &st);
            if(r_stat < 0) {
                printf("ls %s stat error(%d): %s\n", entry->d_name, r_stat, strerror(-r_stat));
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
                printf("  %-4s %s %10ld: %s\n", type, buf, st.st_size, entry->d_name);
            }
        }
    }
}