#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <string.h>

#include <fcntl.h>
#include <dirent.h>

int isFile(const char* name) {
    DIR* directory = opendir(name);
    if (directory != NULL) {
        closedir(directory);
        return 0;
    }
    return 1;
}

int fileExist(const char* filename) {
    struct stat buffer;
    int exist = stat(filename, &buffer);
    if (exist == 0) {
	return 1;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argv[1] == NULL || argv[2] == NULL) {
	exit(1);
    }
    char buffer[1024];
    int file1;
    int file2;
    ssize_t count;
    file1 = open(argv[1], O_RDONLY);
    if (file1 == -1) {
	    exit(1);
    }
    //TODO: consider the situation where argv[2] is a folder
    if (isFile(argv[2]) == 0) {
        if (argv[2][strlen(argv[2] - 1)] != '/') {
            strcat(argv[2], "/");
        }
        char *src = argv[1];
        for (unsigned int i = strlen(argv[1]) - 1; i >= 0; i--) {
            if (argv[1][i] == '/') {
                src += (i + 1);
                break;
            }
        }
        strcat(argv[2], src);
    } else {
        if (fileExist(argv[2])) {
	        remove(argv[2]);
	        printf("Overwrite existing file\n");
        }
    }
    file2 = open(argv[2], O_WRONLY | O_CREAT | O_EXCL | S_IRUSR | S_IWUSR, 0666);
    if (file2 == -1) {
        exit(1);
    }
    while ((count = read(file1, buffer, sizeof(buffer))) != 0) {
	    write(file2, buffer, count);
    }
    exit(0);
}
