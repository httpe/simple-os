#ifndef _KERNEL_CONSOLE_H
#define _KERNEL_CONSOLE_H

#include <kernel/file_system.h>

int console_init(struct file_system* fs);

#endif