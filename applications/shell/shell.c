#include <stdio.h>
#include <unistd.h>

#define FD_STDIN 0
#define FD_STDOUT 1
#define FD_STDERR 2

int main(int argc, char* argv[]) {
    printf("Welcome to the Shell!\n");
    printf("Shell ARGC(%d)\n", argc); 
    for(int i=0; i<argc; i++) {
        printf("  %d: %s\n", i, argv[i]);
    }
   
    char c;
    write(FD_STDOUT, "Input:\n", 7);
    while(1) {
        int read_in = read(FD_STDIN, &c, 1);
        if(read_in == 1) {
            write(FD_STDOUT, &c, 1);
        }
    }

    return 0;
}