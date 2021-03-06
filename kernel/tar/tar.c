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
int tar_lookup(unsigned char* archive, const char* filename, char** out) {
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
bool is_tar_header(unsigned char* archive) {
    return !memcmp(archive + 257, "ustar", 5);
}

// Check if archive is pointing to the start of a tarball meta sector for file named filename
int tar_match_filename(unsigned char* archive, const char* filename) {
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
int tar_get_filesize(unsigned char* archive) {
    if (is_tar_header(archive)) {
        return oct2bin(archive + 0x7c, 11);
    } else {
        return TAR_ERR_NOT_USTAR; // Not USTAR file
    }
}