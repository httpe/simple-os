#ifndef _KERNEL_FILE_SYSTEM_H
#define _KERNEL_FILE_SYSTEM_H

#include <stdint.h>

#include <kernel/block_io.h>
#include <stat.h>
#include <common.h>

////////////////////////////////////////
//
//  FS level structures
//
////////////////////////////////////////


#define N_FILE_SYSTEM_TYPES 2
enum file_system_type {
    FILE_SYSTEM_FAT_32,
    FILE_SYSTEM_US_TAR
};

enum file_system_status {
    FS_STATUS_NOT_INITIALIZED,
    FS_STATUS_READY
};

struct fs_mount_point; // defined below
typedef struct file_system {
    enum file_system_status status;
    enum file_system_type type;
    void* fs_global_meta;
    int (*mount) (struct fs_mount_point* mount_point, void* fs_option);
    int (*unmount) (struct fs_mount_point* mount_point);
} file_system;


////////////////////////////////////////
//
//  FUSE-like FS interface
//
////////////////////////////////////////


// Mimic FUSE struct fuse_file_info
typedef struct fs_file_info {
	/** Open flags.	 Available in open() and release() */
	int flags;
	/** File handle id.  May be filled in by filesystem in create,
	 * open, and opendir().  Available in most other file operations on the
	 * same file handle. */
	uint64_t fh;
} fs_file_info;

// Abstraction of FUSE fuse_fill_dir_t
struct fs_dir_filler_info; // Opaque struct, definition differ in FUSE vs simple-OS 
typedef struct fs_dir_filler_info fs_dir_filler_info;
typedef int (*fs_dir_filler) (fs_dir_filler_info*, const char *name, const struct fs_stat *); // definition differ in FUSE vs simple-OS 

struct fs_mount_point; // Declaring below

// Mimic FUSE struct fuse_operations
typedef struct file_system_operations {
    int (*getattr) (struct fs_mount_point* mount_point, const char * path, struct fs_stat *, struct fs_file_info *);
    int (*mknod) (struct fs_mount_point* mount_point, const char * path, uint mode);
    int (*mkdir) (struct fs_mount_point* mount_point, const char * path, uint mode);
    int (*unlink) (struct fs_mount_point* mount_point, const char * path);
    int (*rmdir) (struct fs_mount_point* mount_point, const char * path);
    int (*rename) (struct fs_mount_point* mount_point, const char * from, const char * to, uint flags);
    int (*truncate) (struct fs_mount_point* mount_point, const char * path, uint size, struct fs_file_info *fi);
    int (*open) (struct fs_mount_point* mount_point, const char * path, struct fs_file_info *);
    int (*read) (struct fs_mount_point* mount_point, const char * path, char *buf, uint size, uint offset, struct fs_file_info *);
	int (*write) (struct fs_mount_point* mount_point, const char * path, const char *buf, uint size, uint offset, struct fs_file_info *);
	int (*release) (struct fs_mount_point* mount_point, const char * path, struct fs_file_info *);
	int (*readdir) (struct fs_mount_point* mount_point, const char * path, uint offset, struct fs_dir_filler_info* filler_info, fs_dir_filler filler);
} file_system_operations;

////////////////////////////////////////
//
//  VFS mount point
//
////////////////////////////////////////

typedef struct fs_mount_option {
    uint flag; // not yet used
} fs_mount_option;

struct file_system_operations;
typedef struct fs_mount_point {
    uint id;
    struct file_system* fs;
    block_storage* storage;
    char* mount_target;
    struct fs_mount_option mount_option;

    void* fs_meta; // File system internal data structure
    struct file_system_operations operations;
} fs_mount_point;





////////////////////////////////////////
//
//  File level structures
//
////////////////////////////////////////


enum file_type {
  FILE_TYPE_INODE
};

typedef struct file {
  enum file_type type;
  char* path;               /* path into the mount point */
  int open_flags;
  struct fs_mount_point* mount_point;        /* Mount Point ID  */
  uint64_t inum;                  /* File serial number.	*/
  int ref;                    /* Reference count */
  char readable;
  char writable;
  uint offset;
} file;


////////////////////////////////////////
//
//  For parsing partition table
//
////////////////////////////////////////


typedef struct mbr_partition_table_entry {
    uint8_t driver_attributes;
    uint8_t CHS_partition_start[3];
    uint8_t partition_type;
    uint8_t CHS_partition_end[3];
    uint32_t LBA_partition_start;
    uint32_t partition_sector_count;
} __attribute__ ((__packed__)) mbr_partition_table_entry;


#endif