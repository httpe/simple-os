#ifndef _KERNEL_VFS_H
#define _KERNEL_VFS_H

#include <kernel/block_io.h>
#include <kernel/file_system.h>
#include <fs.h>
#include <common.h>

// maximum number of mount points
#define N_MOUNT_POINT 16

// maximum number of files opened
#define N_FILE_STRUCTURE 64

int fs_mount(const char* target, enum file_system_type file_system_type, 
            fs_mount_option option, void* fs_option, fs_mount_point** mount_point);
int fs_unmount(const char* mount_root);
int fs_getattr(const char * path, struct fs_stat * stat, int file_idx);
int fs_mknod(const char * path, uint mode);
int fs_mkdir(const char * path, uint mode);
int fs_rmdir(const char * path);
int fs_link(const char* old_path, const char* new_path);
int fs_unlink(const char * path);
int fs_truncate(const char * path, uint size, int file_idx);
int fs_rename(const char * from, const char* to, uint flags);
int fs_readdir(const char * path, uint entry_offset, fs_dirent* buf, uint buf_size);
int fs_open(const char * path, int flags);
int fs_release(int file_idx);
int fs_read(int file_idx, void *buf, uint size);
int fs_seek(int file_idx, int offset, int whence);
int fs_write(int file_idx, void *buf, uint size);
int fs_dupfile(int file_idx);

int init_vfs();

#endif