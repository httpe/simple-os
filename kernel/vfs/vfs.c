
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <kernel/errno.h>
#include <kernel/stat.h>
#include <kernel/block_io.h>
#include <kernel/vfs.h>
#include <kernel/fat.h>
#include <kernel/tar.h>
#include <kernel/process.h>
#include <kernel/console.h>

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
    *remaining_path = NULL;
    if(*path != '/' || strlen(path)==0) {
        // Do not support relative path, yet
        // also we assume there is no mount point attaching to root dir itself
        // all mount points should goes under it
        return NULL;
    }

    // Return the longest prefix matched mount point to allow mount point inside of mounted folder
    // Since we only allow mounting immediately under root dir, this degenrate into a simple match
    int max_match_mount_point_id = -1;
    int max_match_len = 0;
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


int fs_mount(const char* target, enum file_system_type file_system_type, 
            fs_mount_option option, void* fs_option, fs_mount_point** mount_point)
{
    size_t idx, last_slash = -1;
    for(idx=0;idx<strlen(target);idx++) {
        if(target[idx] == '/') {
            last_slash = idx;
        }
    }
    if(last_slash != 0 || idx<=1) {
        // target must be in the form of '/NAME', i.e. has and only has one slash, at the beginning
        // i.e. only allow mounting at a folder under root dir, and also not allowing mounting at root dir itself
        return -1;
    }
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
    // TODO: make sure the mount point already exist in the parent folder
    for(j=0;j<N_MOUNT_POINT;j++) {
        if(mount_points[j].mount_target == NULL) {
            mount_points[j] = (fs_mount_point) {
                .id = next_mount_point_id++,
                .fs = &fs[i],
                .mount_target=strdup(target), 
                .mount_option=option
            };
            int res = fs[i].mount(&mount_points[j], fs_option);
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

int fs_link(const char* old_path, const char* new_path)
{
    const char* remaining_path_old = NULL;
    fs_mount_point* mp_old = find_mount_point(old_path, &remaining_path_old);
    if(mp_old == NULL) {
        return -ENXIO;
    }
    if(mp_old->operations.link == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }

    const char* remaining_path_new = NULL;
    fs_mount_point* mp_new = find_mount_point(new_path, &remaining_path_new);
    if(new_path == NULL) {
        return -ENXIO;
    }
    if(mp_old != mp_new) {
        // cannot link (move) between different mount points
        return -EPERM;
    }

    int res = mp_old->operations.link(mp_old, remaining_path_old, remaining_path_new);
    
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
    if(mp->operations.open != NULL) {
        // if the file system support opening file internally 
        int res = mp->operations.open(mp, remaining_path, &fi);
        if(res < 0) {
            return res;
        }
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

// return: number of entries read into buf
int fs_readdir(const char * path, uint entry_offset, fs_dirent* buf, uint buf_size) 
{
    struct fs_dir_filler_info filler_info = {.buf = buf, .buf_size = buf_size, .entry_written = 0};
    if(strcmp(path,"/") == 0) {
        // if is reading root dir
        // return the mount points
        uint entry_read = 0;
        for(uint i=entry_offset; i<N_MOUNT_POINT; i++) {
            if(mount_points[i].mount_target != NULL) {
                assert(mount_points[i].mount_target[0] == '/');
                // target+1 to skip the first slash
                if(dir_filler(&filler_info, mount_points[i].mount_target+1, NULL) != 0) {
                    // if filler's internal buffer is full, stop
                    break;
                }
                entry_read++;
            }
        }
        return entry_read;
    }
    const char* remaining_path = NULL;
    fs_mount_point* mp = find_mount_point(path, &remaining_path);
    if(mp == NULL) {
        return -ENXIO;
    }
    if(mp->operations.readdir == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }
    
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

int fs_getattr_path(const char * path, struct fs_stat * stat)
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

int fs_getattr_fd(int fd, struct fs_stat * stat)
{
    file* f = fd2file(fd);
    if(f == NULL) {
        return -EBADF;
    }

    if(f->mount_point->operations.getattr == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }

    int res = f->mount_point->operations.getattr(f->mount_point, f->path, stat, NULL);
    
    return res;
}

int fs_dup(int fd)
{
    proc* p = curr_proc();
    file* f = fd2file(fd);
    if(f == NULL) {
        return -EBADF;
    }

    // file descriptor is the index into (one process's) file_table
    int new_fd;
    for(new_fd=0; new_fd<N_FILE_DESCRIPTOR_PER_PROCESS;new_fd++) {
        if(p->files[new_fd] == NULL) {
            break;
        }
    }
    if(new_fd == N_FILE_DESCRIPTOR_PER_PROCESS) {
        // too many opended files
        return -EMFILE;
    }
    p->files[new_fd] = f;
    f->ref++;

    return new_fd;
}

int fs_close(int fd)
{
    proc* p = curr_proc();
    file* f = fd2file(fd);
    if(f == NULL) {
        return -EBADF;
    }

    p->files[fd]->ref--;
    if(p->files[fd]->ref == 0) {
        struct fs_file_info fi = {.flags = f->open_flags, .fh=f->inum};
        if(f->mount_point->operations.release != NULL) {
            // if file system does support closing/release files internally
            int res = f->mount_point->operations.release(f->mount_point, f->path, &fi);
            if(res < 0) {
                return res;
            }
        }

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
    if(!f->readable) {
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
    if(!f->writable) {
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

int fs_fstat(int fd, struct fs_stat * stat)
{
    file* f = fd2file(fd);
    if(f == NULL) {
        return -EBADF;
    }
    if(f->mount_point->operations.getattr == NULL) {
        // if file system does not support this operation
        return -EPERM;
    }

    int res = f->mount_point->operations.getattr(f->mount_point, f->path, stat, NULL);
    
    return res;
}


int init_vfs()
{
    // initialize all supported file systems
    fs[0] = (struct file_system) {.type = FILE_SYSTEM_FAT_32};
    int res = fat32_init(&fs[0]);
    assert(res == 0);
    
    fs[1] = (struct file_system) {.type = FILE_SYSTEM_US_TAR};
    res = tar_init(&fs[1]);
    assert(res == 0);

    fs[2] = (struct file_system) {.type = FILE_SYSTEM_CONSOLE};
    res = console_init(&fs[2]);
    assert(res == 0);

    // mount hda (IDE master drive) to be the root dir (assumed to be US-TAR formated)
	block_storage* storage = get_block_storage(IDE_MASTER_DRIVE);
    tar_mount_option tar_opt = (tar_mount_option) {
        .storage = storage,
        .starting_LBA = BOOTLOADER_SECTORS
    };
    fs_mount_option mount_option = {0};
    fs_mount_point* mp = NULL;
    int32_t mount_res = fs_mount("/boot", FILE_SYSTEM_US_TAR, mount_option, &tar_opt, &mp);
    assert(mount_res == 0);

	// mount hdb (IDE slave drive) to be the home dir (assumed to be FAT-32 formated)
	storage = get_block_storage(IDE_SLAVE_DRIVE);
	if(storage != NULL) {
        fat_mount_option fat_opt = (fat_mount_option) {.storage = storage};
		mount_res = fs_mount("/home", FILE_SYSTEM_FAT_32, mount_option, &fat_opt, &mp);
		assert(mount_res == 0);
	}

    // mount console
    mount_res = fs_mount("/console", FILE_SYSTEM_CONSOLE, mount_option, NULL, &mp);
    assert(mount_res == 0);
    
    return 0;
}