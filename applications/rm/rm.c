#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char* argv[]) {
    if(argc < 2 || argv[1] == NULL) {
        exit(0);
    }
    int r = unlink(argv[1]);
    if(r < 0) {
        perror("rm error");
        exit(1);
    } else {
        exit(0);
    }
}