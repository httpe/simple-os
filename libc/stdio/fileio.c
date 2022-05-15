#include<stdio.h>
#include<string.h>
#include<syscall.h>
#include<common.h>
#include <fsstat.h>


static inline _syscall3(SYS_READ, int, sys_read, int, fd, void*, buf, uint, size)
static inline _syscall3(SYS_WRITE, int, sys_write, int, fd, const void*, buf, uint, size)
static inline _syscall2(SYS_OPEN, int, sys_open, const char*, path, int, flags)
static inline _syscall1(SYS_CLOSE, int, sys_close, int, fd)
static inline _syscall3(SYS_SEEK, int, sys_seek, int, fd, int, offset, int, whence)
static inline _syscall1(SYS_GET_FILE_OFFSET, int, sys_get_file_offset, int, fd)

FILE* fopen(const char *pathname, const char *mode)
{
    // printf("fopen(%s, %s)\n", pathname, mode);
    int r = -1;
    if(strcmp(mode, "r") == 0) {
        r = sys_open(pathname, O_RDONLY);
    } else if(strcmp(mode, "w") == 0){
        r = sys_open(pathname, O_WRONLY|O_CREAT|O_TRUNC);
    }
    if(r < 0) {
        return NULL;
    } else {
        return (FILE*) r;
    }
}
int fclose(FILE *stream)
{
    // printf("fclose(%u)\n", stream);
    return sys_close((int)stream);
}
size_t fread(void * ptr, size_t size, size_t nitems, FILE * stream)
{
    // printf("fread(%u, %u, %u, %u)\n", ptr, size, nitems, stream);
    return sys_read((int)stream, ptr, size*nitems);
}
size_t fwrite(const void * ptr, size_t size, size_t nitems, FILE * stream)
{
    // printf("fwrite(%u, %u, %u, %u)\n", ptr, size, nitems, stream);
    return sys_write((int)stream, ptr, size*nitems);
}
int fseek(FILE *stream, long offset, int whence)
{
    // printf("fseek(%u, %d, %d)\n", stream, offset, whence);
    return sys_seek((int)stream, offset, whence);
}
long ftell(FILE *stream)
{
    // printf("ftell(%u)\n", stream);
    return sys_get_file_offset((int)stream);
}
ssize_t write(int fildes, const void *buf, size_t nbyte)
{
    // if(fildes != 0 && fildes != 1) {
    //     printf("write(%d, %u, %u)\n", fildes, buf, nbyte);
    // }
    return sys_write(fildes, buf, nbyte);
}
