#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char* argv[]) {
    if(argc < 3 || argv[1] == NULL || argv[2] == NULL) {
        exit(1);
    }
    int r = rename(argv[1], argv[2]);
    if(r < 0) {
        perror("mv error");
        exit(1);
    } else {
        exit(0);
    }
}