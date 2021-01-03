#ifndef TAR_H
#define TAR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../string/string.h"

enum tar_error_code {
	TAR_ERR_GENERAL = -1,
	TAR_ERR_NOT_USTAR = -2,
	TAR_ERR_FILE_NAME_NOT_MATCH = -3,
	TAR_ERR_LBA_GT_MAX_SECTOR = -4
};

int tar_lookup(unsigned char *archive, const char *filename, char **out);
bool is_tar_header(unsigned char *archive);
int tar_match_filename(unsigned char *archive, const char *filename);
int tar_get_filesize(unsigned char *archive);

#endif