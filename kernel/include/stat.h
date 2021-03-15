#ifndef _STAT_H
#define _STAT_H

#include <stdint.h>
#include <time.h>

// Mimic Linux stat.h

typedef struct fs_stat {
    uint64_t mount_point_id;        /* Mount Point ID  */
    uint64_t inum;                 /* File serial number.	*/
    uint64_t nlink;		            /* Link count.  */
    uint32_t mode;		            /* File mode.  */
    uint64_t size;		            /* Size of file, in bytes.  */
    uint64_t blocks;	            /* Number 512-byte blocks allocated. */
    date_time mtime;	            /* Time of last modification.  */
    date_time ctime;	            /* Time of last status change.  */
} fs_stat;


// Sourced from Linux stat.h and fcntl.h

/* Encoding of the file mode.  */
#define	__S_IFMT	0170000	/* These bits determine file type.  */

/* File types.  */
#define	__S_IFDIR	0040000	/* Directory.  */
// #define	__S_IFCHR	0020000	/* Character device.  */
// #define	__S_IFBLK	0060000	/* Block device.  */
#define	__S_IFREG	0100000	/* Regular file.  */
// #define	__S_IFIFO	0010000	/* FIFO.  */
// #define	__S_IFLNK	0120000	/* Symbolic link.  */
// #define	__S_IFSOCK	0140000	/* Socket.  */

# define S_IFMT		__S_IFMT
# define S_IFDIR	__S_IFDIR
// # define S_IFCHR	__S_IFCHR
// # define S_IFBLK	__S_IFBLK
# define S_IFREG	__S_IFREG
// # define S_IFIFO	__S_IFIFO
// # define S_IFLNK	__S_IFLNK
// # define S_IFSOCK	__S_IFSOCK


/* Protection bits.  */

// #define	__S_ISUID	04000	/* Set user ID on execution.  */
// #define	__S_ISGID	02000	/* Set group ID on execution.  */
// #define	__S_ISVTX	01000	/* Save swapped text after use (sticky).  */
#define	__S_IREAD	0400	/* Read by owner.  */
#define	__S_IWRITE	0200	/* Write by owner.  */
#define	__S_IEXEC	0100	/* Execute by owner.  */

#define S_IRUSR	__S_IREAD       /* Read by owner.  */
#define S_IWUSR	__S_IWRITE      /* Write by owner.  */
#define S_IXUSR	__S_IEXEC       /* Execute by owner.  */
/* Read, write, and execute by owner.  */
#define S_IRWXU	(__S_IREAD|__S_IWRITE|__S_IEXEC)

#define S_IRGRP	(S_IRUSR >> 3)  /* Read by group.  */
#define S_IWGRP	(S_IWUSR >> 3)  /* Write by group.  */
#define S_IXGRP	(S_IXUSR >> 3)  /* Execute by group.  */
/* Read, write, and execute by group.  */
#define S_IRWXG	(S_IRWXU >> 3)

#define S_IROTH	(S_IRGRP >> 3)  /* Read by others.  */
#define S_IWOTH	(S_IWGRP >> 3)  /* Write by others.  */
#define S_IXOTH	(S_IXGRP >> 3)  /* Execute by others.  */
/* Read, write, and execute by others.  */
#define S_IRWXO	(S_IRWXG >> 3)


#define O_ACCMODE	   0003
#define O_RDONLY	     00
#define O_WRONLY	     01
#define O_RDWR		     02
#define O_CREAT	       0100	/* Not fcntl.  */
#define O_EXCL		   0200	/* Not fcntl.  */
#define O_NOCTTY	   0400	/* Not fcntl.  */
#define O_TRUNC	      01000	/* Not fcntl.  */
#define O_APPEND	  02000

#endif