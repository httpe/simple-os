#ifndef _KERNEL_TAR_H
#define _KERNEL_TAR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <common.h>
#include <kernel/file_system.h>
#include <kernel/block_io.h>

// Copied from bootloader
// The whole bootloader binary is assumed to take the first 16 sectors
// must be in sync of BOOTLOADER_MAX_SIZE in Makefile (of bootloader)
#define BOOTLOADER_SECTORS 16

#define TAR_SECTOR_SIZE 512

#define TAR_MAX_PATH_LEN 254

typedef struct tar_file_header {
	char filename[100];
	uint64_t mode;
	uint64_t owner_id;
	uint64_t group_id;
	char size[12];
	char mod_time[12];
	uint64_t checksum;
	char type;
	char linked_name[100];
	char magic[6];
	char version[2];
	char owner[32];
	char group[32];
	uint64_t device_major;
	uint64_t device_minor;
	char filename_prefix[155];
} tar_file_header;

// Copied from newlib tar.h
/* General definitions */
#define TMAGIC 		"ustar" /* ustar plus null byte. */
#define TMAGLEN 	6 	/* Length of the above. */
#define TVERSION 	"00"	/* 00 without a null byte. */
#define TVERSLEN	2	/* Length of the above. */
/* Typeflag field definitions */
#define REGTYPE 	'0'	/* Regular file. */
#define AREGTYPE	'\0'	/* Regular file. */
#define LNKTYPE		'1'	/* Link. */
#define SYMTYPE		'2'	/* Symbolic link. */
#define CHRTYPE		'3'	/* Character special. */
#define BLKTYPE		'4'	/* Block special. */
#define DIRTYPE		'5'	/* Directory. */
#define FIFOTYPE	'6'	/* FIFO special. */
#define CONTTYPE	'7'	/* Reserved. */
/* Mode field bit definitions (octal) */
#define	TSUID		04000	/* Set UID on execution. */
#define	TSGID		02000	/* Set GID on execution. */
#define	TSVTX		01000	/* On directories, restricted deletion flag. */
#define	TUREAD		00400	/* Read by owner. */
#define	TUWRITE		00200	/* Write by owner. */
#define	TUEXEC		00100	/* Execute/search by owner. */
#define	TGREAD		00040	/* Read by group. */
#define	TGWRITE		00020	/* Write by group. */
#define	TGEXEC		00010	/* Execute/search by group. */
#define	TOREAD		00004	/* Read by other. */
#define	TOWRITE		00002	/* Write by other. */
#define	TOEXEC		00001	/* Execute/search by other. */

typedef struct tar_mount_option {
	block_storage* storage;
    uint starting_LBA;
} tar_mount_option;

enum tar_error_code {
	TAR_ERR_GENERAL = -1,
	TAR_ERR_NOT_USTAR = -2,
	TAR_ERR_FILE_NAME_NOT_MATCH = -3,
	TAR_ERR_LBA_GT_MAX_SECTOR = -4
};


int tar_init(struct file_system* fs);

#endif