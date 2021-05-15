#include <stdlib.h>
#include <string.h>
#include <kernel/tar.h>
#include <kernel/errno.h>

// From https://wiki.osdev.org/USTAR

// Convert octal string to integer
static int oct2bin(unsigned char* str, int size) {
    int n = 0;
    unsigned char* c = str;
    while (size-- > 0) {
        n *= 8;
        n += *c - '0';
        c++;
    }
    return n;
}

// Get file name for a path under dir
// If dir is not the prefix of path, return NULL
// If path is a file in a subfolder of dir, return NULL
// If path is the same as dir, return NULL
// Otherwise return the file name
static const char* get_filename(const char* dir, const char* path)
{
    size_t lendir = strlen(dir),
           lenpath = strlen(path);

    if(lendir >= lenpath) {
        return NULL;
    }

    if(memcmp(dir, path, lendir) != 0) {
        return NULL;
    }

    // Support dir ending with or without '/'
    int offset = 1;
    if(dir[lendir-1]=='/') {
        offset = 0;
    }
 
    for(uint i=lendir + offset; i<lenpath; i++) {
        // filter out files in sub-folders
        // offset: for case dir="/d", path="/d/a", skip the second '/'
        if(path[i] == '/' && i!=lenpath-1) {
            return NULL;
        }
    }

    if(strlen(&path[lendir+offset]) == 0) {
        return NULL;
    }

    return &path[lendir+offset];
}


// Check if archive is pointing to the start of a tarball meta sector
static bool is_tar_header(tar_file_header* header) {
    return !memcmp(header->magic, "ustar", 5);
}

static int is_same_path(const char* path1, const char* path2)
{
    if(path1 == path2) {
        return 1;
    }
    if(path1 == NULL || path2 == NULL) {
        return 0;
    }
    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);
    if(path1[len1-1] == '/') {
        len1--;
    }
    if(path2[len2-1] == '/') {
        len2--;
    }
    if(len1 != len2) {
        return 0;
    }
    int r = memcmp(path1, path2, len1);
    return r==0;
}

// Check if archive is pointing to the start of a tarball meta sector for file named filename
static int tar_match_filename(tar_file_header* header, const char* filename) {
    char path[TAR_MAX_PATH_LEN+1] = {0};
    size_t matchlen = strlen(filename);
    if (is_tar_header(header) && matchlen > 0) {
        int prefix_len = strlen(header->filename_prefix);
        if(prefix_len > 0) {
            memmove(path, header->filename_prefix, prefix_len);
        }
        size_t namelen = strlen(header->filename);
        memmove(path + prefix_len, header->filename, namelen);
        path[prefix_len + namelen] = 0;
        
        if (is_same_path(path, filename)) {
            return 0;
        } else {
            return TAR_ERR_FILE_NAME_NOT_MATCH; // Filename not match
        }
    } else {
        return TAR_ERR_NOT_USTAR; // Not USTAR file
    }
}

// Get the actual content size of a file in a tarball
//
// archive: pointer to the start of a tarball meta sector
static int tar_get_filesize(tar_file_header* header) {
    if (is_tar_header(header)) {
        return oct2bin((unsigned char*) header->size, 11);
    } else {
        return TAR_ERR_NOT_USTAR; // Not USTAR file
    }
}


static int tar_loopup_lazy(fs_mount_point *mp, const char* filename, uint* content_LBA, tar_file_header** header) {
    tar_mount_option* opt = (tar_mount_option*) mp->fs_meta;

    unsigned char* sector_buffer = malloc(TAR_SECTOR_SIZE);

    uint LBA = opt->starting_LBA;
    while(1) {
        if (LBA >= (uint32_t) opt->storage->block_count) {
            free(sector_buffer);
            return TAR_ERR_LBA_GT_MAX_SECTOR;
        }
        opt->storage->read_blocks(opt->storage, sector_buffer, LBA, 1);
        int match = tar_match_filename((tar_file_header*) sector_buffer, filename);
        if (match == TAR_ERR_NOT_USTAR) {
            free(sector_buffer);
            return TAR_ERR_NOT_USTAR;
        } else {
            int filesize = tar_get_filesize((tar_file_header*) sector_buffer);
            int size_in_sector = ((filesize + (TAR_SECTOR_SIZE-1)) / TAR_SECTOR_SIZE) + 1; // plus one for the meta sector
            if (match == TAR_ERR_FILE_NAME_NOT_MATCH) {
                LBA += size_in_sector;
                continue;
            } else {
                *content_LBA = LBA + 1;
                free(sector_buffer);
                if(header != NULL) {
                    *header = (tar_file_header*) sector_buffer;
                }
                return filesize;
            }
        }
    }
}


static int tar_read(struct fs_mount_point* mp, const char * path, char *buf, uint size, uint offset, struct fs_file_info *fi)
{
    UNUSED_ARG(fi);

    tar_mount_option* opt = (tar_mount_option*) mp->fs_meta;

    uint content_LBA;
    int filesize = tar_loopup_lazy(mp, path, &content_LBA, NULL);
    if(filesize < 0) {
        return -ENOENT;
    }
    if(filesize == 0) {
        return 0;
    }

    if(offset >= (uint) filesize) {
        return 0;
    }
    if(offset + size > (uint) filesize) {
        size = filesize - offset;
    }

    int size_in_sector = ((size + (TAR_SECTOR_SIZE-1)) / TAR_SECTOR_SIZE) + 1; // plus one for the meta sector
    int LBA = content_LBA + offset / TAR_SECTOR_SIZE;
    char* full_buf = malloc(size_in_sector*TAR_SECTOR_SIZE);
    memset(full_buf, 0, size_in_sector*TAR_SECTOR_SIZE);
    int res = opt->storage->read_blocks(opt->storage, full_buf, LBA, size_in_sector);
    if(res != size_in_sector*TAR_SECTOR_SIZE) {
        return -1;
    }
    int in_block_offset = offset % TAR_SECTOR_SIZE;
    memmove(buf, full_buf + in_block_offset, size);

    return 0;
}


static int tar_getattr(struct fs_mount_point* mount_point, const char * path, struct fs_stat *st, struct fs_file_info *fi)
{
    UNUSED_ARG(fi);

    memset(st, 0, sizeof(*st));

    if(strcmp(path, "/") == 0) {
        // For root dir
        st->mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
        st->nlink = 2;
        st->inum = 0;
        st->size = TAR_SECTOR_SIZE;
        st->blocks = st->size/512;
        return 0;
    }

    uint content_LBA;
    tar_file_header* header;
    int size = tar_loopup_lazy(mount_point, path, &content_LBA, &header);
    if(size < 0) {
        return -ENOENT;
    }

    st->mode = S_IRWXU | S_IRWXG | S_IRWXO;
    if (header->type == DIRTYPE) {
        st->mode |= S_IFDIR;
        st->nlink = 2;
        st->inum = content_LBA;
        st->size = size;
        st->blocks = st->size/512;
    } else {
        st->mode |= S_IFREG;
        st->nlink = 1;
        st->inum = content_LBA;
        st->size = size;
        st->blocks = st->size/512;
    }

    return 0;
}


static int tar_readdir(struct fs_mount_point* mp, const char * path, uint offset, struct fs_dir_filler_info* info, fs_dir_filler filler)
{
    tar_mount_option* opt = (tar_mount_option*) mp->fs_meta;

    unsigned char* sector_buffer = malloc(TAR_SECTOR_SIZE);

    uint LBA = opt->starting_LBA;
    uint file_idx = 0;
    uint dir_ent_read = 0;
    while(1) {
        if (LBA >= (uint32_t) opt->storage->block_count) {
            free(sector_buffer);
            return dir_ent_read;
        }
        opt->storage->read_blocks(opt->storage, sector_buffer, LBA, 1);
        if (!is_tar_header((tar_file_header*) sector_buffer)) {
            free(sector_buffer);
            return dir_ent_read;
        } else {
            const char* filename = get_filename(path, (char*) sector_buffer);
            int filesize = tar_get_filesize((tar_file_header*) sector_buffer);
            int size_in_sector = ((filesize + (TAR_SECTOR_SIZE-1)) / TAR_SECTOR_SIZE) + 1; // plus one for the meta sector
            if (filename != NULL) {
                if(file_idx >= offset ) {
                    filler(info, filename, NULL);
                    dir_ent_read++;
                }
                file_idx++;
            }
            LBA += size_in_sector;
        }
    }
}

static int tar_mount(struct fs_mount_point* mount_point, void* option)
{
    tar_mount_option* opt_in = (tar_mount_option*) option;
    if(opt_in->storage->block_size != 512) {
        return -EIO;
    }

    // internalize the mounting option
    tar_mount_option* opt = malloc(sizeof(*opt_in));
    memmove(opt, option, sizeof(tar_mount_option));
    mount_point->fs_meta = opt;
    
    mount_point->operations = (struct file_system_operations) {
        .read = tar_read,
        .getattr = tar_getattr,
        .readdir = tar_readdir
    };

    return 0;
}

static int tar_unmount(struct fs_mount_point* mount_point)
{
    free(mount_point->fs_meta);
    return 0;
}

int tar_init(struct file_system* fs)
{
    fs->mount = tar_mount;
    fs->unmount = tar_unmount;
    fs->fs_global_meta = NULL;
    fs->status = FS_STATUS_READY;
    return 0;
}