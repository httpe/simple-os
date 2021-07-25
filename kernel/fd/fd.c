#include <kernel/fd.h>
#include <kernel/vfs.h>
#include <kernel/errno.h>

static int alloc_fd(file* f)
{
    // file descriptor is the index into (one process's) file_table
    proc* p = curr_proc();
    for(int fd=0; fd<N_FILE_DESCRIPTOR_PER_PROCESS;fd++) {
        if(p->files[fd] == NULL) {
            p->files[fd] = f;
            return fd;
        }
    }
    // too many opended files
    return -EMFILE;
}

file* fd2file(int fd)
{
    proc* p = curr_proc();
    if(fd < 0 || fd >= N_FILE_DESCRIPTOR_PER_PROCESS) {
        return NULL;
    }
    file* f = p->files[fd];
    if(f == NULL || f->ref == 0) {
        return NULL;
    }
    return f;
}

int dup_fd(int fd)
{
    file* f = fd2file(fd);
    if(f == NULL) return -1;
    int r = alloc_fd(f);
    if(r < 0) return r;
    return fs_dupfile(f);
}

int close_fd(int fd)
{
    proc* p = curr_proc();
    file* f = fd2file(fd);
    if(f == NULL) return -EBADF;
    int r = fs_close(f);
    if(r < 0) return r;
    p->files[fd] = NULL;
    return 0;
}

int open_fd(const char * path, int flags)
{
    file* f = NULL;
    int r = fs_open(path, flags, &f);
    if(r < 0) return r;
    return alloc_fd(f);
}

int write_fd(int fd, void *buf, uint size)
{
    file* f = fd2file(fd);
    if(f == NULL) return -EBADF;
    return fs_write(f, buf, size);
}
int read_fd(int fd, void *buf, uint size)
{
    file* f = fd2file(fd);
    if(f == NULL) return -EBADF;
    return fs_read(f, buf, size);
}
int seek_fd(int fd, int offset, int whence)
{
    file* f = fd2file(fd);
    if(f == NULL) return -EBADF;
    return fs_seek(f, offset, whence);
}
int getattr_fd(int fd, struct fs_stat * stat)
{
    file* f = fd2file(fd);
    if(f == NULL) return -EBADF;
    return fs_getattr(f->path, stat, f);
}
int truncate_fd(int fd, uint size)
{
    file* f = fd2file(fd);
    if(f == NULL) return -EBADF;
    return fs_truncate(NULL, size, f);
}
int close_all_fd()
{
    proc* p = curr_proc();
    for(int fd=0; fd<N_FILE_DESCRIPTOR_PER_PROCESS; fd++) {
        if(p->files[fd] != NULL) {
            int r = close_fd(fd);
            if(r < 0) return -r;
        }
    }
    return 0;
}
int copy_fd(proc* from, proc* to)
{
    for(int i=0;i<N_FILE_DESCRIPTOR_PER_PROCESS;i++) {
        if(from->files[i] != NULL) {
            int r = fs_dupfile(from->files[i]);
            if(r < 0) return r;
            to->files[i] = from->files[i];
        }
    }
    return 0;
}
