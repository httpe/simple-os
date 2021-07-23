#ifndef _KERNEL_PIPE_H
#define _KERNEL_PIPE_H

#include <kernel/file_system.h>

#define N_MAX_PIPE 64

int pipe_init(struct file_system* fs);

#endif