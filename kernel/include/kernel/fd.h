#ifndef _KERNEL_FD_H
#define _KERNEL_FD_H

// File descriptor management

#include <kernel/file_system.h>
#include <kernel/process.h>

int dup_fd(int fd);

int close_all_fd();
int copy_fd(proc* from, proc* to);

int open_fd(const char * path, int flags);
int close_fd(int fd);
int write_fd(int fd, void *buf, uint size);
int read_fd(int fd, void *buf, uint size);
int seek_fd(int fd, int offset, int whence);
int getattr_fd(int fd, struct fs_stat * stat);
int truncate_fd(int fd, uint size);

#endif
