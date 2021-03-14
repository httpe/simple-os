
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <stat.h>
#include <errno.h>

#include <kernel/block_io.h>
#include <kernel/vfs.h>
#include <kernel/fat.h>
#include <kernel/process.h>


#define N_MOUNT_POINT 16
#define N_FILE_STRUCTURE 12


static uint next_mount_point_id = 1;

static fs_mount_point mount_points[N_MOUNT_POINT];
static file_system fs[N_FILE_SYSTEM_TYPES];

static const char* root_path = "/";

// Global (kernel) file table for all opened files
static struct {
  file file[N_FILE_STRUCTURE];
} file_table;


//////////////////////////////////////

fs_mount_point* find_mount_point(const char* path, const char**remaining_path)
{
    // Return the longest prefix matched mount point to allow mount point inside of mounted folder
    int max_match_mount_point_id = -1;
    int max_match_len = 0;
    *remaining_path = NULL;
    if(*path != '/') {
        // Do not support relative path, yet
        return NULL;
    }
    for(int i=0;i<N_MOUNT_POINT;i++) {
        if(mount_points[i].mount_target != NULL) {
            int match_len = 0;
            const char* p = path;
            const char* m = mount_points[i].mount_target;
            if(strcmp(m, root_path) == 0) {
                // mount = "/"
                if(max_match_len < 1) {
                    max_match_mount_point_id = i;
                    max_match_len = 1;
                    *remaining_path = p;
                }
                continue;
            }
            while(*p != 0 && *m != 0 && *p == *m) {
                p++;
                m++;
                match_len++;
            }
            if(*p == 0 && *m == 0) {
                // path = "/abc", mount = "/abc"
                *remaining_path = root_path;
                return &mount_points[i];
            }
            if(*p == '/' && *m == 0) {
                // path = "/abc/xyz", mount = "/abc"
                assert(max_match_len != match_len);
                if(max_match_len < match_len) {
                    max_match_mount_point_id = i;
                    max_match_len = match_len;
                    *remaining_path = p;
                }
            }
        }
    }
    if(max_match_mount_point_id == -1) {
        return NULL;
    } else {
        return &mount_points[max_match_mount_point_id];
    }
}


int fs_mount(block_storage* storage, const char* target, enum file_system_type file_system_type, 
            fs_mount_option option, void* fs_option, fs_mount_point** mount_point)
{
    int i;
    for(i=0; i<N_FILE_SYSTEM_TYPES; i++) {
        if(fs[i].type == file_system_type && fs[i].status == FS_STATUS_READY) {
            break;
        }
    }
    if(i == N_FILE_SYSTEM_TYPES) {
        // file system not found
        return -1;
    }
    int j;
    for(j=0;j<N_MOUNT_POINT;j++) {
        if(mount_points[j].mount_target != NULL) {
            if(strcmp(mount_points[j].mount_target, target) == 0) {
                // target already mounted
                return -1;
            }
        }
    }
    for(j=0;j<N_MOUNT_POINT;j++) {
        if(mount_points[j].mount_target == NULL) {
            mount_points[j] = (fs_mount_point) {
                .id = next_mount_point_id++,
                .fs = &fs[i],
                .storage = storage,
                .mount_target=strdup(target), 
                .mount_option=option, 
                .fs_option = fs_option
            };
            int res = fs[i].mount(&mount_points[j]);
            if(res < 0) {
                free(mount_points[j].mount_target);
                memset(&mount_points[j], 0, sizeof(mount_points[j]));
                *mount_point = NULL;
                return res;
            }
            *mount_point = &mount_points[j];
            return 0;
        }
    }
    // No mount point available
    return -1;
}

int fs_unmount(const char* mount_root)
{
    const char* remaining_path = NULL;
    fs_mount_point* mp = find_mount_point(mount_root, &remaining_path);
    if(mp == NULL || strcmp(remaining_path, root_path) != 0) {
        return -ENXIO;
    }
    int res = mp->fs->unmount(mp);
    if(res < 0) {
        return res;
    }
    memset(mp, 0, sizeof(*mp));
    return 0;
}

int fs_getattr(const char * path, struct fs_stat * stat)
{
    const char* remaining_path = NULL;
    fs_mount_point* mp = find_mount_point(path, &remaining_path);
    if(mp == NULL) {
        return -ENXIO;
    }
    if(mp->operations.getattr == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }

    int res = mp->operations.getattr(mp, remaining_path, stat, NULL);
    
    return res;
}

int fs_mknod(const char * path, uint mode)
{
    const char* remaining_path = NULL;
    fs_mount_point* mp = find_mount_point(path, &remaining_path);
    if(mp == NULL) {
        return -ENXIO;
    }
    if(mp->operations.mknod == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }

    int res = mp->operations.mknod(mp, remaining_path, mode);
    
    return res;
}

int fs_mkdir(const char * path, uint mode)
{
    const char* remaining_path = NULL;
    fs_mount_point* mp = find_mount_point(path, &remaining_path);
    if(mp == NULL) {
        return -ENXIO;
    }
    if(mp->operations.mkdir == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }

    int res = mp->operations.mkdir(mp, remaining_path, mode);
    
    return res;
}

int fs_rmdir(const char * path)
{
    const char* remaining_path = NULL;
    fs_mount_point* mp = find_mount_point(path, &remaining_path);
    if(mp == NULL) {
        return -ENXIO;
    }
    if(mp->operations.rmdir == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }

    int res = mp->operations.rmdir(mp, remaining_path);
    
    return res;
}

int fs_unlink(const char * path)
{
    const char* remaining_path = NULL;
    fs_mount_point* mp = find_mount_point(path, &remaining_path);
    if(mp == NULL) {
        return -ENXIO;
    }
    if(mp->operations.unlink == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }

    int res = mp->operations.unlink(mp, remaining_path);
    
    return res;
}

int fs_truncate(const char * path, uint size)
{
    const char* remaining_path = NULL;
    fs_mount_point* mp = find_mount_point(path, &remaining_path);
    if(mp == NULL) {
        return -ENXIO;
    }
    if(mp->operations.truncate == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }

    int res = mp->operations.truncate(mp, remaining_path, size, NULL);
    
    return res;
}

int fs_rename(const char * from, const char* to, uint flags)
{
    const char* remaining_path_from = NULL;
    fs_mount_point* mp_from = find_mount_point(from, &remaining_path_from);
    if(mp_from == NULL) {
        return -ENXIO;
    }
    if(mp_from->operations.rename == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }

    const char* remaining_path_to = NULL;
    fs_mount_point* mp_to = find_mount_point(to, &remaining_path_to);
    if(mp_to == NULL) {
        return -ENXIO;
    }
    if(mp_to != mp_from) {
        // cannot rename (move) between different mount points
        return -EPERM;
    }

    int res = mp_from->operations.rename(mp_from, remaining_path_from, remaining_path_to, flags);
    
    return res;
}

int fs_open(const char * path, int flags)
{
    const char* remaining_path = NULL;
    fs_mount_point* mp = find_mount_point(path, &remaining_path);
    if(mp == NULL) {
        return -ENXIO;
    }
    if(mp->operations.open == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }

    // allocate kernel file structure
    file* f = NULL; 
    for(int i=0;i<N_FILE_STRUCTURE;i++) {
        if(file_table.file[i].ref == 0) {
            f = &file_table.file[i];
            break;
        }
    }
    if(f == NULL) {
        // No available file structure cache
        return -ENFILE;
    }

    // file descriptor is the index into (one process's) file_table
    proc* p = curr_proc();
    int fd;
    for(fd=0; fd<N_FILE_DESCRIPTOR_PER_PROCESS;fd++) {
        if(p->files[fd] == NULL) {
            break;
        }
    }
    if(fd == N_FILE_DESCRIPTOR_PER_PROCESS) {
        // too many opended files
        return -EMFILE;
    }

    fs_file_info fi = {.flags = flags, .fh=0};
    int res = mp->operations.open(mp, remaining_path, &fi);
    if(res < 0) {
        return res;
    }
    *f = (file) {
        .inum = fi.fh, // file system's internal inode number / file handler number
        .open_flags = flags,
        .mount_point = mp,
        .offset = 0,
        .ref = 1,
        .path = strdup(remaining_path), //storing path relative to the mount point
        .readable = !(flags & O_WRONLY),
        .writable = (flags & O_WRONLY) || (flags & O_RDWR)
    };
    p->files[fd] = f;

    // Return file descriptor
    return fd;
}

struct fs_dir_filler_info {
    void* buf;
    uint buf_size;
    uint entry_written;
};

static int dir_filler(fs_dir_filler_info* filler_info, const char *name, const struct fs_stat *st)
{
    (void) st;

    if((filler_info->entry_written + 1)*sizeof(fs_dirent) > filler_info->buf_size) {
        // buffer full
        return 1;
    } else {
        uint len = strlen(name);
        if(len > FS_MAX_FILENAME_LEN) {
            len = FS_MAX_FILENAME_LEN;
        }
        fs_dirent* dirent = (fs_dirent*) (filler_info->buf + filler_info->entry_written * sizeof(fs_dirent));
        memmove(dirent->name, name, len+1);
        filler_info->entry_written++;
        return 0;
    }
}

int fs_readdir(const char * path, uint entry_offset, fs_dirent* buf, uint buf_size) 
{
    const char* remaining_path = NULL;
    fs_mount_point* mp = find_mount_point(path, &remaining_path);
    if(mp == NULL) {
        return -ENXIO;
    }
    if(mp->operations.readdir == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }
    struct fs_dir_filler_info filler_info = {.buf = buf, .buf_size = buf_size, .entry_written = 0};
    int res = mp->operations.readdir(mp, remaining_path, entry_offset, &filler_info, dir_filler);
    if(res < 0) {
        return res;
    }

    return filler_info.entry_written;
}

static file* fd2file(int fd)
{
    proc* p = curr_proc();
    if(fd < 0 || fd >= N_FILE_STRUCTURE || p->files[fd] == NULL || p->files[fd]->ref == 0) {
        return NULL;
    }
    file* f = p->files[fd];
    return f;
}

int fs_close(int fd)
{
    proc* p = curr_proc();
    file* f = fd2file(fd);
    if(f == NULL) {
        return -EBADF;
    }
    if(f->mount_point->operations.release == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }

    struct fs_file_info fi = {.flags = f->open_flags, .fh=f->inum};
    int res = f->mount_point->operations.release(f->mount_point, f->path, &fi);
    if(res < 0) {
        return res;
    }

    p->files[fd]->ref--;
    if(p->files[fd]->ref == 0) {
        free(p->files[fd]->path);
        memset(f, 0, sizeof(*f));
    }

    p->files[fd] = NULL;
    
    return 0;
}

int fs_read(int fd, void *buf, uint size)
{
    file* f = fd2file(fd);
    if(f == NULL) {
        return -EBADF;
    }
    if(f->mount_point->operations.read == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }

    struct fs_file_info fi = {.flags = f->open_flags, .fh=f->inum};
    int res = f->mount_point->operations.read(f->mount_point, f->path, buf, size, f->offset, &fi);
    if(res < 0) {
        return res;
    }
    f->offset += res;
    
    return res;
}

int fs_write(int fd, void *buf, uint size)
{
    file* f = fd2file(fd);
    if(f == NULL) {
        return -EBADF;
    }
    if(f->mount_point->operations.write == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }

    struct fs_file_info fi = {.flags = f->open_flags, .fh=f->inum};
    int res = f->mount_point->operations.write(f->mount_point, f->path, buf, size, f->offset, &fi);
    if(res < 0) {
        return res;
    }
    f->offset += res;
    
    return res;
}

int fs_seek(int fd, int offset, int whence)
{
    file* f = fd2file(fd);
    if(f == NULL) {
        return -EBADF;
    }

    if(whence == SEEK_WHENCE_CUR) {
        f->offset += offset;
    } else if(whence == SEEK_WHENCE_SET) {
        f->offset = offset;
    } else if(whence == SEEK_WHENCE_END) {
        fs_stat st = {0};
        if(f->mount_point->operations.getattr == NULL) {
            // if file system does not support this operation
            return -EPERM;
        }
        int res = f->mount_point->operations.getattr(f->mount_point, f->path, &st, NULL);
        if(res<0){
            return res;
        }
        f->offset = st.size + offset;
    } else {
        return -EPERM;
    }

    return 0;
    
}

int init_vfs()
{
    // initialize all supported file systems
    fs[0] = (struct file_system) {.type = FILE_SYSTEM_FAT_32};
    int res = fat32_init(&fs[0]);
    
    return res;
}