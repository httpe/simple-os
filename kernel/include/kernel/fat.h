
#ifndef _KERNEL_FAT_H
#define _KERNEL_FAT_H

#include <stdint.h>
#include <kernel/lock.h>
#include <kernel/block_io.h>
#include <kernel/file_system.h>

// Source: https://wiki.osdev.org/FAT

typedef struct fat32_bootsector
{
	uint8_t 		bootjmp[3];
	uint8_t 		oem_name[8];
	uint16_t 	    bytes_per_sector;
	uint8_t		    sectors_per_cluster;
	uint16_t		reserved_sector_count;
	uint8_t		    table_count;
	uint16_t		root_entry_count;
	uint16_t		total_sectors_16;
	uint8_t		    media_type;
	uint16_t		table_sector_size_16;
	uint16_t		sectors_per_track;
	uint16_t		head_side_count;
	uint32_t     	hidden_sector_count;
	uint32_t     	total_sectors_32;
 
	//extended fat32 stuff
	uint32_t	    table_sector_size_32; // FAT size in sectors
	uint16_t		extended_flags;
	uint16_t		fat_version;
	uint32_t		root_cluster;
	uint16_t		fs_info_sector;
	uint16_t		backup_BS_sector;
	uint8_t 		reserved_0[12];
	uint8_t		    drive_number;
	uint8_t 		reserved_1;
	uint8_t		    boot_signature;
	uint32_t 		volume_id;
	uint8_t		    volume_label[11];
	uint8_t		    fat_type_label[8];
    uint8_t         boot_code[420];
    uint16_t        mbr_signature;
}__attribute__((packed)) fat32_bootsector;

// Source: https://github.com/aroulin/FAT32-FS-Driver
#define FAT_SHORT_NAME_LEN 8
#define FAT_SHORT_EXT_LEN 3
typedef struct fat32_direntry_short {
	/* 0*/	union {
			struct {
				uint8_t		name[FAT_SHORT_NAME_LEN];
				uint8_t		ext[FAT_SHORT_EXT_LEN];
			};
			uint8_t			nameext[FAT_SHORT_NAME_LEN+FAT_SHORT_EXT_LEN];
		};
	/*11*/	uint8_t		attr;
	/*12*/	uint8_t		res;
	/*13*/	uint8_t		ctime_ms;
	/*14*/	uint16_t	ctime_time;
	/*16*/	uint16_t	ctime_date;
	/*18*/	uint16_t	atime_date;
	/*20*/	uint16_t	cluster_hi;
	/*22*/	uint16_t	mtime_time;
	/*24*/	uint16_t	mtime_date;
	/*26*/	uint16_t	cluster_lo;
	/*28*/	uint32_t	size;
} __attribute__ ((__packed__)) fat32_direntry_short;

#define FAT32_USC2_FILE_NAME_LEN_PER_LFN 13
typedef struct fat32_direntry_long {
	/* 0*/	uint8_t		seq;
	/* 1*/	uint16_t	name1[5];
	/*11*/	uint8_t		attr;
	/*12*/	uint8_t		type;
	/*13*/	uint8_t		csum;
	/*14*/	uint16_t	name2[6];
	/*26*/	uint16_t	reserved2;
	/*28*/	uint16_t	name3[2];
} __attribute__ ((__packed__)) fat32_direntry_long;

typedef union {
    fat32_direntry_short short_entry;
    fat32_direntry_long long_entry;
} fat32_direntry_t;

#define FAT32_MAX_LFN_ENTRY_PER_FILE 0x14
#define FAT32_LONG_NAME_MAX_LEN_USC2 (FAT32_USC2_FILE_NAME_LEN_PER_LFN * FAT32_MAX_LFN_ENTRY_PER_FILE)
#define FAT32_FILENAME_SIZE (FAT32_LONG_NAME_MAX_LEN_USC2*2)
typedef struct fat32_file_entry {
    fat32_direntry_short direntry;
    char filename[FAT32_LONG_NAME_MAX_LEN_USC2*2+1];// +1: space for null terminator
	// uint32_t dir_entry_cluster_start; // the cluster containing the start of this entry
	// uint32_t dir_entry_idx_start; // idx relative to the cluster
	uint32_t dir_cluster; // first cluster of the dir
	uint32_t first_dir_entry_idx; // an index into all clusters constitute the dir
	uint32_t dir_entry_count;
} fat32_file_entry;


typedef enum {
	FAT_ATTR_READ_ONLY = 0x01,
	FAT_ATTR_HIDDEN = 0x02,
	FAT_ATTR_SYSTEM = 0x04,
	FAT_ATTR_VOLUME_ID = 0x08,
	FAT_ATTR_DIRECTORY = 0x10,
	FAT_ATTR_ARCHIVE = 0x20,
	FAT_ATTR_LFN = 0x0F
} fat_attr;

// Ref: http://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/fatgen103.doc
typedef struct fat32_fsinfo {
    uint32_t lead_signature;
    uint8_t reserved[480];
    uint32_t structure_signature;
    uint32_t free_cluster_count;
    uint32_t next_free_cluster;
    uint8_t reserved2[12];
    uint32_t trailing_signature;
} __attribute__ ((__packed__)) fat32_fsinfo;

#define FAT32_N_OPEN_FILE 100
typedef struct fat32_meta {
    fat32_bootsector* bootsector;
    fat32_fsinfo* fs_info;
    uint32_t* fat;
	block_storage* storage;
	fat32_file_entry* file_table;
	rw_lock rw_lk;
} fat32_meta;

typedef struct fat_cluster {
	uint32_t curr;
	uint32_t next;
} fat_cluster;

typedef enum fat_cluster_status {
    FAT_CLUSTER_FREE = 0,
    FAT_CLUSTER_BAD = 0x00000007,
    FAT_CLUSTER_RESERVED = 0x00000001,
    FAT_CLUSTER_EOC = 0xFFFFFFFF,
    FAT_CLUSTER_USED = 0x00000002
} fat_cluster_status;

typedef struct fat_dir_iterator {
	uint32_t first_cluster;
	uint32_t dir_entry_count;
	uint32_t entry_per_cluster;
	uint32_t current_dir_entry_idx; // an index into all clusters constitute the dir
	fat32_direntry_t* dir_entries;
} fat_dir_iterator;

typedef enum fat_iterate_dir_status {
	FAT_DIR_ITER_VALID_ENTRY,
	FAT_DIR_ITER_DOT_ENTRY,
	FAT_DIR_ITER_DELETED,
	FAT_DIR_ITER_NO_MORE_ENTRY,
	FAT_DIR_ITER_FREE_ENTRY,
	FAT_DIR_ITER_ERROR
} fat_iterate_dir_status;

typedef enum fat_resolve_path_status {
	FAT_PATH_RESOLVE_ROOT_DIR,
	FAT_PATH_RESOLVE_FOUND,
	FAT_PATH_RESOLVE_ERROR,
	FAT_PATH_RESOLVE_NOT_FOUND,
	FAT_PATH_RESOLVE_INVALID_PATH
} fat_resolve_path_status;

enum fat32_rm_type {
    FAT32_RM_FILE,
    FAT32_RM_DIR,
    FAT32_RM_ANY
};

typedef struct fat_mount_option {
	block_storage* storage;
} fat_mount_option;


int fat32_init(struct file_system* fs);

// Used by make_fs
void fat32_set_timestamp(uint16_t* date_entry, uint16_t* time_entry);

#endif
