#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

int main(int argc, char* argv[]) {
    if(argc < 2 || argv[1] == NULL) {
        exit(1);
    }
    mode_t mode = 0;
    if(argc > 2 || argv[2] != NULL) {
        mode = (mode_t) atol(argv[2]);
    }
    int r = mkdir(argv[1], mode);
    if(r < 0) {
        perror("mkdir error");
        exit(1);
    } else {
        exit(0);
    }
}