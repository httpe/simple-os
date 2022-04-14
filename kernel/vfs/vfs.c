
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <kernel/errno.h>
#include <kernel/stat.h>
#include <kernel/block_io.h>
#include <kernel/vfs.h>
#include <kernel/fat.h>
#include <kernel/tar.h>
#include <kernel/process.h>
#include <kernel/console.h>
#include <kernel/pipe.h>
#include <kernel/lock.h>

static const char* root_path = "/";

static struct {
    uint next_mount_point_id;
    fs_mount_point mount_points[N_MOUNT_POINT];
    file_system fs[N_FILE_SYSTEM_TYPES];
    file file_table[N_FILE_STRUCTURE]; // Global (kernel) file table for all opened files
    yield_lock lk;
} vfs;


//////////////////////////////////////

static fs_mount_point* find_mount_point(const char* path, const char**remaining_path)
{
    *remaining_path = NULL;
    if(*path != '/' || strlen(path)==0) {
        // Relative path shall be converted to absolute path before passing in
        return NULL;
    }

    acquire(&vfs.lk);
    fs_mount_point* mp = NULL;
    // Return the longest prefix matched mount point to allow mount point inside of mounted folder
    int max_match_mount_point_id = -1;
    int max_match_len = 0;
    for(int i=0;i<N_MOUNT_POINT;i++) {
        if(vfs.mount_points[i].mount_target != NULL) {
            int match_len = 0;
            const char* p = path;
            const char* m = vfs.mount_points[i].mount_target;
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
                mp =  &vfs.mount_points[i];
                release(&vfs.lk);
                return mp;
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
        release(&vfs.lk);
        mp = NULL;
    } else {
        mp =  &vfs.mount_points[max_match_mount_point_id];
    }
    release(&vfs.lk);
    return mp;
}


int fs_mount(const char* target, enum file_system_type file_system_type, 
            fs_mount_option option, void* fs_option, fs_mount_point** mount_point)
{
    if(*target != '/') {
        // Relative path shall be converted to absolute path before passing in
        return -EINVAL;
    }

    if(strcmp(target, "/") != 0) {
        // Make sure the mount point already exist in the parent folder
        fs_stat st = {0};
        int res_getattr = fs_getattr(target, &st, -1);
        if(res_getattr < 0) {
            return res_getattr;
        }
    }

    int ret = -EPERM;
    acquire(&vfs.lk);

    int i;
    for(i=0; i<N_FILE_SYSTEM_TYPES; i++) {
        if(vfs.fs[i].type == file_system_type && vfs.fs[i].status == FS_STATUS_READY) {
            break;
        }
        if(i == N_FILE_SYSTEM_TYPES - 1) {ret = -EEXIST; goto end;}
    }
    int j;
    for(j=0;j<N_MOUNT_POINT;j++) {
        if(vfs.mount_points[j].mount_target != NULL) {
            if(strcmp(vfs.mount_points[j].mount_target, target) == 0) {
                // target already mounted
                ret = -EEXIST;
                goto end;
            }
        }
    }

    for(j=0;j<N_MOUNT_POINT;j++) {
        if(vfs.mount_points[j].mount_target == NULL) {
            vfs.mount_points[j] = (fs_mount_point) {
                .id = vfs.next_mount_point_id++,
                .fs = &vfs.fs[i],
                .mount_target=strdup(target), 
                .mount_option=option
            };
            int res = vfs.fs[i].mount(&vfs.mount_points[j], fs_option);
            if(res < 0) {
                free(vfs.mount_points[j].mount_target);
                memset(&vfs.mount_points[j], 0, sizeof(vfs.mount_points[j]));
                ret = res;
                goto end;
            }
            *mount_point = &vfs.mount_points[j];
            ret = 0;
            goto end;
        }
    }
    // No mount point available
    ret = -1;

end:
    release(&vfs.lk);
    return ret;
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
    //TODO: lock mount point for unmount
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

    int ret = -EPERM;
    acquire(&vfs.lk);

    // allocate kernel file structure
    file* f = NULL; 
    for(int i=0;i<N_FILE_STRUCTURE;i++) {
        if(vfs.file_table[i].ref == 0) {
            f = &vfs.file_table[i];
            ret = i;
            break;
        }
    }
    if(f == NULL) {
        // No available file structure cache
        ret = -ENFILE;
        goto end;
    }

    fs_file_info fi = {.flags = flags, .fh=0};
    if(mp->operations.open != NULL) {
        // if the file system support opening file internally 
        int res = mp->operations.open(mp, remaining_path, &fi);
        if(res < 0) {
            ret = res;
            goto end;
        }
    } else if (mp->operations.getattr != NULL) {
        fs_stat st;
        int res = mp->operations.getattr(mp, remaining_path, &st, &fi);
        if(res < 0) {
            ret = res;
            goto end;
        }
    }

    *f = (file) {
        .type = FILE_TYPE_INODE,
        .inum = fi.fh, // file system's internal inode number / file handler number
        .open_flags = flags,
        .mount_point = mp,
        .offset = 0,
        .ref = 1,
        .path = strdup(remaining_path), //storing path relative to the mount point
        .readable = !(flags & O_WRONLY),
        .writable = (flags & O_WRONLY) || (flags & O_RDWR)
    };

end:
    release(&vfs.lk);
    return ret;
}

file* idx2file(int file_idx)
{
    if(file_idx < 0 || file_idx >= N_FILE_STRUCTURE) {
        return NULL;
    }
    file* f = &vfs.file_table[file_idx];
    if(f->ref == 0) {
        return NULL;
    }
    return f;
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

int fs_getattr(const char * path, struct fs_stat * stat, int file_idx)
{
    const char* remaining_path;
    fs_mount_point* mp;
    struct fs_file_info fi;
    struct fs_file_info* pfi = NULL;
    file* opened_file = idx2file(file_idx);
    if(opened_file == NULL) {
        return -ENOENT;
    }
    if(opened_file) {
        remaining_path = opened_file->path;
        mp = opened_file->mount_point;
        fi = (fs_file_info) {.flags = opened_file->open_flags, .fh=opened_file->inum};
        pfi = &fi;
    } else {
        mp = find_mount_point(path, &remaining_path);
        if(mp == NULL) return -ENXIO;
    }

    if(mp->operations.getattr == NULL) return -EPERM;
    int res = mp->operations.getattr(mp, remaining_path, stat, pfi);
    
    return res;
}

int fs_truncate(const char * path, uint size, int file_idx)
{
    const char* remaining_path;
    fs_mount_point* mp;
    struct fs_file_info fi;
    struct fs_file_info* pfi = NULL;
    file* opened_file = idx2file(file_idx);
    if(opened_file == NULL) {
        return -ENOENT;
    }
    if(opened_file) {
        remaining_path = opened_file->path;
        mp = opened_file->mount_point;
        fi = (fs_file_info) {.flags = opened_file->open_flags, .fh=opened_file->inum};
        pfi = &fi;
    } else {
        mp = find_mount_point(path, &remaining_path);
        if(mp == NULL) return -ENXIO;
    }

    if(mp->operations.truncate == NULL) return -EPERM;
    int res = mp->operations.truncate(mp, remaining_path, size, pfi);
    
    return res;
}

int fs_dupfile(int file_idx)
{
    acquire(&vfs.lk);
    file* f = idx2file(file_idx);
    if(f == NULL) {
        return -ENOENT;
    }
    f->ref++;
    release(&vfs.lk);

    return 0;
}

int fs_release(int file_idx)
{

    acquire(&vfs.lk);
    file* f = idx2file(file_idx);
    if(f == NULL) {
        release(&vfs.lk);
        return -ENOENT;
    }
    if(!f) return -1;
    f->ref--;
    if(f->ref == 0) {
        struct fs_file_info fi = {.flags = f->open_flags, .fh=f->inum};
        if(f->mount_point->operations.release != NULL) {
            // if file system does support closing/release files internally
            int res = f->mount_point->operations.release(f->mount_point, f->path, &fi);
            if(res < 0) {
                release(&vfs.lk);
                return res;
            }
        }

        free(f->path);
        memset(f, 0, sizeof(*f));
    }

    release(&vfs.lk);
    return 0;
}

int fs_read(int file_idx, void *buf, uint size)
{
    file* f = idx2file(file_idx);
    if(f == NULL) {
        return -ENOENT;
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

int fs_write(int file_idx, void *buf, uint size)
{
    file* f = idx2file(file_idx);
    if(f == NULL) {
        return -ENOENT;
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

int fs_seek(int file_idx, int offset, int whence)
{
    file* f = idx2file(file_idx);
    if(f == NULL) {
        return -ENOENT;
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

int fs_tell(int file_idx)
{
    file* f = idx2file(file_idx);
    if(f == NULL) {
        return -ENOENT;
    }
    return f->offset;
}



int init_vfs()
{
    // initialize all supported file systems
    vfs.fs[0] = (struct file_system) {.type = FILE_SYSTEM_FAT_32};
    int res = fat32_init(&vfs.fs[0]);
    assert(res == 0);
    
    vfs.fs[1] = (struct file_system) {.type = FILE_SYSTEM_US_TAR};
    res = tar_init(&vfs.fs[1]);
    assert(res == 0);

    vfs.fs[2] = (struct file_system) {.type = FILE_SYSTEM_CONSOLE};
    res = console_init(&vfs.fs[2]);
    assert(res == 0);

    vfs.fs[3] = (struct file_system) {.type = FILE_SYSTEM_PIPE};
    res = pipe_init(&vfs.fs[3]);
    assert(res == 0);

    // mount hda (IDE master drive) to be the root dir (assumed to be US-TAR formated)
	block_storage* storage = get_block_storage(IDE_MASTER_DRIVE);
    tar_mount_option tar_opt = (tar_mount_option) {
        .storage = storage,
        .starting_LBA = BOOTLOADER_SECTORS
    };
    fs_mount_option mount_option = {.mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO};
    fs_mount_point* mp = NULL;
    int32_t mount_res = fs_mount("/", FILE_SYSTEM_US_TAR, mount_option, &tar_opt, &mp);
    assert(mount_res == 0);

	// mount hdb (IDE slave drive) to be the home dir (assumed to be FAT-32 formated)
	storage = get_block_storage(IDE_SLAVE_DRIVE);
	if(storage != NULL) {
        fat_mount_option fat_opt = (fat_mount_option) {.storage = storage};
		// the existence of /home is guaranteed by the install-reserved-path target of kernel Makefile 
        mount_res = fs_mount("/home", FILE_SYSTEM_FAT_32, mount_option, &fat_opt, &mp);
		assert(mount_res == 0);
	}

    // mount console
    // the existence of /console is guaranteed by the install-reserved-path target of kernel Makefile 
    mount_option = (fs_mount_option) {.mode = S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO};
    mount_res = fs_mount("/console", FILE_SYSTEM_CONSOLE, mount_option, NULL, &mp);
    assert(mount_res == 0);
    
    // mount pipe
    // the existence of /pipe is guaranteed by the install-reserved-path target of kernel Makefile 
    mount_option = (fs_mount_option) {.mode = S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO};
    mount_res = fs_mount("/pipe", FILE_SYSTEM_PIPE, mount_option, NULL, &mp);
    assert(mount_res == 0);

    
    return 0;
}