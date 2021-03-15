#ifndef _KERNEL_TAR_H
#define _KERNEL_TAR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <common.h>
#include <kernel/file_system.h>

// Copied from bootloader
// The whole bootloader binary is assumed to take the first 16 sectors
// must be in sync of BOOTLOADER_MAX_SIZE in Makefile (of bootloader)
#define BOOTLOADER_SECTORS 16

#define TAR_SECTOR_SIZE 512

typedef struct tar_mount_option {
    uint starting_LBA;
} tar_mount_option;

enum tar_error_code {
	TAR_ERR_GENERAL = -1,
	TAR_ERR_NOT_USTAR = -2,
	TAR_ERR_FILE_NAME_NOT_MATCH = -3,
	TAR_ERR_LBA_GT_MAX_SECTOR = -4
};

int tar_lookup(unsigned char* archive, const char* filename, char** out);
bool is_tar_header(unsigned char* archive);
int tar_match_filename(unsigned char* archive, const char* filename);
int tar_get_filesize(unsigned char* archive);

int tar_init(struct file_system* fs);

#endif