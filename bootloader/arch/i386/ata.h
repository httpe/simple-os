#ifndef ARCH_I386_ATA_H
#define ARCH_I386_ATA_H

#include<stdint.h>
#include "port_io.h"

// 28 bit ATA PIO disk driver
// From http://learnitonweb.com/2020/05/22/12-developing-an-operating-system-tutorial-episode-6-ata-pio-driver-osdev/
// Source - https://wiki.osdev.org/ATA_PIO_Mode#x86_Directions

/*
 BSY: a 1 means that the controller is busy executing a command. No register should be accessed (except the digital output register) while this bit is set.
RDY: a 1 means that the controller is ready to accept a command, and the drive is spinning at correct speed..
WFT: a 1 means that the controller detected a write fault.
SKC: a 1 means that the read/write head is in position (seek completed).
DRQ: a 1 means that the controller is expecting data (for a write) or is sending data (for a read). Don't access the data register while this bit is 0.
COR: a 1 indicates that the controller had to correct data, by using the ECC bytes (error correction code: extra bytes at the end of the sector that allows to verify its integrity and, sometimes, to correct errors).
IDX: a 1 indicates the the controller retected the index mark (which is not a hole on hard-drives).
ERR: a 1 indicates that an error occured. An error code has been placed in the error register.
*/

#define STATUS_BSY 0x80
#define STATUS_RDY 0x40
#define STATUS_DRQ 0x08
#define STATUS_DF 0x20
#define STATUS_ERR 0x01

void read_sectors_ATA_28bit_PIO(uint16_t* target, uint32_t LBA, uint8_t sector_count);
void write_sectors_ATA_28bit_PIO(uint32_t LBA, uint8_t sector_count, uint16_t* words);

uint8_t ATA_Identify(uint16_t* target);


#endif