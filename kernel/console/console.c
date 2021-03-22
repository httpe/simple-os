#include <stddef.h>
#include <errno.h>
#include <kernel/console.h>
#include <kernel/keyboard.h>
#include <kernel/tty.h>

static int console_read(struct fs_mount_point* mount_point, const char * path, char *buf, uint size, uint offset, struct fs_file_info *fi)
{
    UNUSED_ARG(mount_point);
    UNUSED_ARG(path);
    UNUSED_ARG(offset);
    UNUSED_ARG(fi);

    uint char_read = 0;
    while(char_read < size) {
        char c = read_key_buffer();
        if(c == 0) {
            return char_read;
        }
        *buf = c;
        buf++;
        char_read++; 
    }
    return char_read;
}

static int console_write(struct fs_mount_point* mount_point, const char * path, const char *buf, uint size, uint offset, struct fs_file_info *fi)
{
    UNUSED_ARG(mount_point);
    UNUSED_ARG(path);
    UNUSED_ARG(offset);
    UNUSED_ARG(fi);

    for(uint i=0; i<size; i++) {
        terminal_putchar(buf[i]);
    }
    return 0;
}

static int console_mount(struct fs_mount_point* mount_point, void* option)
{
    UNUSED_ARG(option);
    mount_point->operations = (struct file_system_operations) {
        .read = console_read,
        .write = console_write,
    };

    return 0;
}

static int console_unmount(struct fs_mount_point* mount_point)
{
    UNUSED_ARG(mount_point);
    return 0;
}


int console_init(struct file_system* fs)
{
    fs->mount = console_mount;
    fs->unmount = console_unmount;
    fs->fs_global_meta = NULL;
    fs->status = FS_STATUS_READY;
    return 0;
}