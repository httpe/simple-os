#ifndef _KERNEL_VFS_H
#define _KERNEL_VFS_H

#include <kernel/block_io.h>
#include <kernel/file_system.h>
#include <fs.h>

int32_t fs_mount(block_storage* storage, const char* target, enum file_system_type file_system_type, 
            fs_mount_option option, void* fs_option, fs_mount_point** mount_point);
int32_t fs_unmount(const char* mount_root);
int64_t fs_getattr(const char * path, struct fs_stat * stat);
int64_t fs_mknod(const char * path, uint32_t mode);
int64_t fs_mkdir(const char * path, uint32_t mode);
int64_t fs_rmdir(const char * path);
int64_t fs_unlink(const char * path);
int64_t fs_truncate(const char * path, int64_t size);
int64_t fs_rename(const char * from, const char* to, uint32_t flags);
int32_t fs_open(const char * path, int32_t flags);
int32_t fs_readdir(const char * path, int64_t entry_offset, fs_dirent* buf, uint32_t buf_size);
int32_t fs_close(int32_t fd);
int64_t fs_read(int32_t fd, void *buf, uint32_t size);
int64_t fs_seek(int32_t fd, int32_t offset, int32_t whence);
int64_t fs_write(int32_t fd, void *buf, uint32_t size);

fs_mount_point* find_mount_point(const char* path, const char**remaining_path);
int32_t init_vfs();

#endif