#ifndef _FS_H
#define _FS_H

#include <stdint.h>

// In line with newlib's unistd.h
#define SEEK_WHENCE_SET 0
#define SEEK_WHENCE_CUR 1
#define SEEK_WHENCE_END 2

#define FS_MAX_FILENAME_LEN 260
typedef struct fs_dirent {
    uint32_t inum; 
    char name[FS_MAX_FILENAME_LEN]; // file name, max len = 260 + null terminator (referencing FAT32 max file name)
} fs_dirent;

#endif