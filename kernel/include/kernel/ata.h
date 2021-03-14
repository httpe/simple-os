#ifndef _KERNEL_ATA_H
#define _KERNEL_ATA_H

#include <stdint.h>
#include <stdbool.h>

void read_sectors_ATA_PIO(bool slave, void* buf, uint32_t LBA, uint32_t sector_count);
void write_sectors_ATA_PIO(bool slave, const void* buf, uint32_t LBA, uint32_t sector_count);
int32_t get_total_28bit_sectors(bool slave);

#endif