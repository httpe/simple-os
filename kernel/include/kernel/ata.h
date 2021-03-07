#ifndef _KERNEL_ATA_H
#define _KERNEL_ATA_H

#include <stdint.h>
#include <stdbool.h>

void read_sectors_ATA_28bit_PIO(bool slave, uint16_t* target, uint32_t LBA, uint8_t sector_count);
void write_sectors_ATA_28bit_PIO(bool slave, uint32_t LBA, uint8_t sector_count, uint16_t* words);

int8_t ATA_Identify(bool slave, uint16_t* target);
uint32_t get_total_28bit_sectors(bool slave);

#endif