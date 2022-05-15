#include <kernel/pipe.h>
#include <fsstat.h>
#include <kernel/process.h>
#include <kernel/errno.h>
#include <kernel/lock.h>
#include <stdlib.h>
#include <stdint.h>
#include <common.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

typedef struct pipe {
    uint id;
    char* name; // if is empty string "", regard as unamed pipe
    char* buf;
    int size;
    int r;
    int w;
    uint ref;
    yield_lock lk;
} pipe;

typedef struct pipe_meta {
    uint next_pipe_id;
    pipe* all_pipe;
    yield_lock lk;
} pipe_meta;

static pipe* name2pipe(pipe_meta* meta, const char* name, uint* idx) {
    for(int i=0; i< N_MAX_PIPE; i++) {
        pipe* p = &meta->all_pipe[i];
        if(p->ref) {
            // skip "/" which means unamed pipe
            if(strcmp(p->name, "/") != 0 && strcmp(name, p->name) == 0) {
                if(idx != NULL) {
                    *idx = i;
                }
                return p;
            }
        }
    }
    return NULL;
}

static int free_space(pipe* p) {
    if(p->r == p->w) {
        return p->size - 1;
    }
    if(p->r < p->w) {
        return (p->size - p->w) + p->r - 1;
    } else {
        return p->r - p->w - 1;
    }
}

static int bytes_ready(pipe* p) {
    return p->size - free_space(p) - 1;
}

static int pipe_read(struct fs_mount_point* mount_point, const char * path, char *buf, uint size, uint offset, struct fs_file_info *fi)
{
    UNUSED_ARG(path);
    UNUSED_ARG(offset);

    if(fi == NULL) {
        // only allow writting into opened pipe
        return -EPERM;
    }

    pipe_meta* meta = (pipe_meta*) mount_point->fs_meta;
    acquire(&meta->lk);
    pipe* p = &meta->all_pipe[fi->fh];
    acquire(&p->lk);
    release(&meta->lk);

    // if(size == 0) {
    //     // size zero means read all available
    //     size = bytes_ready(p);
    // }

    uint read_in = 0;
    while(read_in < size) {
        if(bytes_ready(p) == 0)  {
            // buffer is empty
            release(&p->lk);
            yield();
            acquire(&p->lk);
            // // use offset to choose blocking vs non-blocking behavior
            // if(offset == 0) {
            //     // block until the pipe get written
            //     // this way, it is even possible to have size > p->size
            //     yield();
            // } else {
            //   break
            // }
        } else {
            (*buf++) = p->buf[p->r++];
            read_in++;
            if(p->r >= p->size) {
                p->r = 0;
            }
        }
    }
    release(&p->lk);
    return read_in;
}

static int pipe_write(struct fs_mount_point* mount_point, const char * path, const char *buf, uint size, uint offset, struct fs_file_info *fi)
{
    UNUSED_ARG(path);
    UNUSED_ARG(offset);

    if(fi == NULL) {
        // only allow writting into opened pipe
        return -EPERM;
    }

    pipe_meta* meta = (pipe_meta*) mount_point->fs_meta;
    acquire(&meta->lk);
    pipe* p = &meta->all_pipe[fi->fh];
    acquire(&p->lk);
    release(&meta->lk);

    uint written = 0;
    while(written < size) {
        if(free_space(p) == 0)  {
            // buffer is full
            release(&p->lk);
            yield();
            acquire(&p->lk);
            // if(offset == 0) {
            //     // block until the pipe get read
            //     yield();
            // } else {
            //     break;
            // } 
        } else {
            p->buf[p->w++] = *buf++;
            written++;
            if(p->w >= p->size) {
                p->w = 0;
            }
        }
    }
    release(&p->lk);
    return written;
}

static int pipe_getattr(struct fs_mount_point* mount_point, const char * path, struct fs_stat * st, struct fs_file_info *fi)
{
    pipe* p;
    pipe_meta* meta = (pipe_meta*) mount_point->fs_meta;
    memset(st, 0, sizeof(*st));
    acquire(&meta->lk);
    if(fi) {
        // check fi first in order to do getattr on opened unnamed pipe
        p = &meta->all_pipe[fi->fh];
    } else if(strcmp("/", path) == 0) {
        st->mode = S_IRWXU | S_IRWXG | S_IRWXO | S_IFIFO;
        release(&meta->lk);
        return 0;
    } else {
        p = name2pipe(meta, path, NULL);
    }

    if(p) {
        st->size = bytes_ready(p);
        st->mode = S_IRWXU | S_IRWXG | S_IRWXO | S_IFIFO;
        release(&meta->lk);
        return 0;
    } else {
        release(&meta->lk);
        return -ENOENT;
    }
}

static int pipe_open(struct fs_mount_point* mount_point, const char * path, struct fs_file_info *fi)
{
    uint idx;
    pipe_meta* meta = (pipe_meta*) mount_point->fs_meta;

    acquire(&meta->lk);
    pipe* p0 = name2pipe(meta, path, &idx);
    if(p0) {
        p0->ref++;
        fi->fh = idx;
    } else {
        for(int i=0; i< N_MAX_PIPE; i++) {
            pipe* p = &meta->all_pipe[i];
            if(p->ref == 0) {
                p->id = meta->next_pipe_id++;
                p->name = strdup(path);
                // use high bits of flags to represent pipe size
                uint size = ((uint) fi->flags) & ~ (uint) 0xF;
                if(size <= 1) {
                    release(&meta->lk);
                    return -EINVAL;
                }
                p->size = size;
                p->buf = malloc(size);
                p->r = 0;
                p->w = 0;
                p->ref = 1;
                
                fi->fh = i;
                break;
            }
        }
    }
    release(&meta->lk);
    return 0;
}

static int pipe_release(struct fs_mount_point* mount_point, const char * path, struct fs_file_info *fi)
{
	(void) path;
    (void) fi;
    
    if(fi == NULL) {
        return -EPERM;
    }

    pipe_meta* meta = (pipe_meta*) mount_point->fs_meta;
    acquire(&meta->lk);
    pipe* p = &meta->all_pipe[fi->fh];

    if(p) {
        acquire(&p->lk);
        p->ref--;
        if(p->ref == 0) {
            // no need to release p->lk here because it will be freed
            free(p->name);
            free(p->buf);
            memset(p, 0, sizeof(*p));
        } else {
            release(&p->lk);
        }
    }
    release(&meta->lk);
	return 0;
}

static int pipe_mount(struct fs_mount_point* mount_point, void* option)
{
    UNUSED_ARG(option);
    mount_point->operations = (struct file_system_operations) {
        .read = pipe_read,
        .write = pipe_write,
        .getattr = pipe_getattr,
        .open = pipe_open,
        .release = pipe_release
    };

    pipe_meta* meta = malloc(sizeof(pipe_meta));
    memset(meta, 0, sizeof(*meta));
    meta->all_pipe = malloc(N_MAX_PIPE * sizeof(pipe));
    memset(meta->all_pipe, 0, N_MAX_PIPE * sizeof(pipe));

    mount_point->fs_meta = meta;

    return 0;
}

static int pipe_unmount(struct fs_mount_point* mount_point)
{
    pipe_meta* meta = (pipe_meta*) mount_point->fs_meta;
    for(int i=0; i<N_MAX_PIPE; i++) {
        pipe* p = &meta->all_pipe[i];
        if(p->ref) {
            // cannot unmount if there is any pipe still open
            return -1;
        }
    }
    free(meta->all_pipe);
    free(meta);
    mount_point->fs_meta = NULL;

    return 0;
}


int pipe_init(struct file_system* fs)
{
    fs->mount = pipe_mount;
    fs->unmount = pipe_unmount;

    fs->fs_global_meta = NULL;
    fs->status = FS_STATUS_READY;

    return 0;
}