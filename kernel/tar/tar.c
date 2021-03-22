#include <stdlib.h>
#include <string.h>
#include <kernel/tar.h>

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


// returns file size and pointer to file data in out
// Assuming the tar ball is loaded into memory entirely
// 
// archive: point to the start of tarball meta sector in memory
// filename: the file name to match
// out: will point to a pointer of the matched file in the tarball
//
// return: file size 
static int tar_lookup(unsigned char* archive, const char* filename, char** out) {
    unsigned char* ptr = archive;

    while (!memcmp(ptr + 257, "ustar", 5)) {
        int filesize = oct2bin(ptr + 0x7c, 11);
        if (!memcmp(ptr, filename, strlen(filename) + 1)) {
            *out = (char*)ptr + 512;
            return filesize;
        }
        ptr += (((filesize + 511) / 512) + 1) * 512;
    }
    return 0;
}

// Check if archive is pointing to the start of a tarball meta sector
static bool is_tar_header(unsigned char* archive) {
    return !memcmp(archive + 257, "ustar", 5);
}

// Check if archive is pointing to the start of a tarball meta sector for file named filename
static int tar_match_filename(unsigned char* archive, const char* filename) {
    if (is_tar_header(archive)) {
        if (!memcmp(archive, filename, strlen(filename) + 1)) {
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
static int tar_get_filesize(unsigned char* archive) {
    if (is_tar_header(archive)) {
        return oct2bin(archive + 0x7c, 11);
    } else {
        return TAR_ERR_NOT_USTAR; // Not USTAR file
    }
}


static int tar_loopup_lazy(fs_mount_point *mp, const char* filename, uint* content_LBA) {
    tar_mount_option* opt = (tar_mount_option*) mp->fs_meta;

    unsigned char* sector_buffer = malloc(TAR_SECTOR_SIZE);

    uint LBA = opt->starting_LBA;
    while(1) {
        if (LBA >= (uint32_t) opt->storage->block_count) {
            free(sector_buffer);
            return TAR_ERR_LBA_GT_MAX_SECTOR;
        }
        opt->storage->read_blocks(opt->storage, sector_buffer, LBA, 1);
        int match = tar_match_filename(sector_buffer, filename);
        if (match == TAR_ERR_NOT_USTAR) {
            free(sector_buffer);
            return TAR_ERR_NOT_USTAR;
        } else {
            int filesize = tar_get_filesize(sector_buffer);
            int size_in_sector = ((filesize + (TAR_SECTOR_SIZE-1)) / TAR_SECTOR_SIZE) + 1; // plus one for the meta sector
            if (match == TAR_ERR_FILE_NAME_NOT_MATCH) {
                LBA += size_in_sector;
                continue;
            } else {
                *content_LBA = LBA + 1;
                free(sector_buffer);
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
    int filesize = tar_loopup_lazy(mp, path, &content_LBA);
    if(filesize < 0) {
        return -1;
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
    uint content_LBA;
    int size = tar_loopup_lazy(mount_point, path, &content_LBA);
    if(size < 0) {
        return -1;
    }

    // only size property is implemented
    st->size = size;

    return 0;
}


static int tar_mount(struct fs_mount_point* mount_point, void* option)
{
    tar_mount_option* opt_in = (tar_mount_option*) option;
    if(opt_in->storage->block_size != 512) {
        return -1;
    }

    // internalize the mounting option
    tar_mount_option* opt = malloc(sizeof(*opt_in));
    memmove(opt, option, sizeof(tar_mount_option));
    mount_point->fs_meta = opt;
    
    mount_point->operations = (struct file_system_operations) {
        .read = tar_read,
        .getattr = tar_getattr,
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