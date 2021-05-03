#ifndef _STAT_H
#define _STAT_H

#include <stdint.h>
#include <kernel/time.h>

// Mimic Linux stat.h

typedef struct fs_stat {
    uint64_t mount_point_id;        /* Mount Point ID  */
    uint64_t inum;                 /* File serial number. */
    uint64_t nlink;              /* Link count.  */
    uint32_t mode;              /* File mode.  */
    uint64_t size;              /* Size of file, in bytes.  */
    uint64_t blocks;             /* Number 512-byte blocks allocated. */
    date_time mtime;             /* Time of last modification.  */
    date_time ctime;             /* Time of last status change.  */
} fs_stat;


// Sourced from Newlib sys/stat.h

#define _IFMT 0170000   /* type of file */
#define _IFDIR 0040000  /* directory */
#define _IFCHR 0020000  /* character special */
#define _IFBLK 0060000  /* block special */
#define _IFREG 0100000  /* regular */
#define _IFLNK 0120000  /* symbolic link */
#define _IFSOCK 0140000 /* socket */
#define _IFIFO 0010000  /* fifo */

#define S_IFMT  _IFMT
#define S_IFDIR  _IFDIR
#define S_IFCHR  _IFCHR
#define S_IFBLK  _IFBLK
#define S_IFREG  _IFREG
#define S_IFLNK  _IFLNK
#define S_IFSOCK _IFSOCK
#define S_IFIFO  _IFIFO

#define	S_IRWXU 	(S_IRUSR | S_IWUSR | S_IXUSR)
#define		S_IRUSR	0000400	/* read permission, owner */
#define		S_IWUSR	0000200	/* write permission, owner */
#define		S_IXUSR 0000100/* execute/search permission, owner */
#define	S_IRWXG		(S_IRGRP | S_IWGRP | S_IXGRP)
#define		S_IRGRP	0000040	/* read permission, group */
#define		S_IWGRP	0000020	/* write permission, grougroup */
#define		S_IXGRP 0000010/* execute/search permission, group */
#define	S_IRWXO		(S_IROTH | S_IWOTH | S_IXOTH)
#define		S_IROTH	0000004	/* read permission, other */
#define		S_IWOTH	0000002	/* write permission, other */
#define		S_IXOTH 0000001/* execute/search permission, other */

#define	S_ISBLK(m)	(((m)&_IFMT) == _IFBLK)
#define	S_ISCHR(m)	(((m)&_IFMT) == _IFCHR)
#define	S_ISDIR(m)	(((m)&_IFMT) == _IFDIR)
#define	S_ISFIFO(m)	(((m)&_IFMT) == _IFIFO)
#define	S_ISREG(m)	(((m)&_IFMT) == _IFREG)
#define	S_ISLNK(m)	(((m)&_IFMT) == _IFLNK)
#define	S_ISSOCK(m)	(((m)&_IFMT) == _IFSOCK)

// Copied from Newlib sys/_default_fcntl.h

#define O_RDONLY 0  /* +1 == FREAD */
#define O_WRONLY 1  /* +1 == FWRITE */
#define O_RDWR  2  /* +1 == FREAD|FWRITE */

#define _FAPPEND 0x0008 /* append (writes guaranteed at the end) */
#define _FCREAT  0x0200 /* open with file create */
#define _FTRUNC  0x0400 /* open with truncation */
#define _FEXCL  0x0800 /* error on open if file exists */
#define O_APPEND _FAPPEND
#define O_CREAT  _FCREAT
#define O_TRUNC  _FTRUNC
#define O_EXCL  _FEXCL

#endif