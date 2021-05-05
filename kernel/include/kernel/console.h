#ifndef _KERNEL_CONSOLE_H
#define _KERNEL_CONSOLE_H

#include <kernel/file_system.h>

#define CONSOLE_BUF_SIZE 255

int console_init(struct file_system* fs);

#endif