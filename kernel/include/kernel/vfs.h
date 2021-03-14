#ifndef _KERNEL_VFS_H
#define _KERNEL_VFS_H

#include <kernel/block_io.h>
#include <kernel/file_system.h>
#include <fs.h>
#include <common.h>

int fs_mount(block_storage* storage, const char* target, enum file_system_type file_system_type, 
            fs_mount_option option, void* fs_option, fs_mount_point** mount_point);
int fs_unmount(const char* mount_root);
int fs_getattr(const char * path, struct fs_stat * stat);
int fs_mknod(const char * path, uint mode);
int fs_mkdir(const char * path, uint mode);
int fs_rmdir(const char * path);
int fs_unlink(const char * path);
int fs_truncate(const char * path, int64_t size);
int fs_rename(const char * from, const char* to, uint flags);
int fs_open(const char * path, int flags);
int fs_readdir(const char * path, int64_t entry_offset, fs_dirent* buf, uint buf_size);
int fs_close(int fd);
int fs_read(int fd, void *buf, uint size);
int fs_seek(int fd, int offset, int whence);
int fs_write(int fd, void *buf, uint size);

fs_mount_point* find_mount_point(const char* path, const char**remaining_path);
int init_vfs();

#endif