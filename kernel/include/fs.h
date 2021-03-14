#ifndef _FS_H
#define _FS_H

#define SEEK_WHENCE_SET 1
#define SEEK_WHENCE_CUR 2
#define SEEK_WHENCE_END 3

#define FS_MAX_FILENAME_LEN 260
typedef struct fs_dirent {
    char name[FS_MAX_FILENAME_LEN]; // file name, max len = 260 + null terminator (referencing FAT32 max file name)
} fs_dirent;

#endif