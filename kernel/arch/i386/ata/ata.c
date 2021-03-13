#include <kernel/ata.h>
#include <arch/i386/kernel/port_io.h>

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


static void ATA_wait_BSY();
static void ATA_wait_DRQ();
static void ATA_delay_400ns();

// Read sectors from the primary ATA device using 28bit PIO method 
//
// target: a buffer at least sector_count*512 bytes long 
// LBA: 0-based Linear Block Address, 28bit LBA shall be between 0 to 0x0FFFFFFF
// sector_count: How many sectors you want to read. A sectorcount of 0 means 256 sectors = 128K
//
void read_sectors_ATA_28bit_PIO(bool slave, uint16_t* target, uint32_t LBA, uint8_t sector_count) {
    ATA_wait_BSY();
    // Send 0xE0 for the "master" or 0xF0 for the "slave", ORed with the highest 4 bits of the LBA to port 0x1F6: outb(0x1F6, 0xE0 | (slavebit << 4) | ((LBA >> 24) & 0x0F))
    outb(0x1F6, 0xE0 | (slave << 4) | ((LBA >> 24) & 0xF));
    // Send the sectorcount to port 0x1F2: outb(0x1F2, (unsigned char) count)
    outb(0x1F2, sector_count);
    // Send the low 8 bits of the LBA to port 0x1F3: outb(0x1F3, (unsigned char) LBA))
    outb(0x1F3, (uint8_t)LBA);
    // Send the next 8 bits of the LBA to port 0x1F4: outb(0x1F4, (unsigned char)(LBA >> 8))
    outb(0x1F4, (uint8_t)(LBA >> 8));
    // Send the next 8 bits of the LBA to port 0x1F5: outb(0x1F5, (unsigned char)(LBA >> 16))
    outb(0x1F5, (uint8_t)(LBA >> 16));
    // Send the "READ SECTORS" command (0x20) to port 0x1F7: outb(0x1F7, 0x20)
    outb(0x1F7, 0x20); //Send the read command

    for (int j = 0;j < sector_count;j++) {
        // Poll for status
        ATA_delay_400ns();
        ATA_wait_BSY();
        ATA_wait_DRQ();
        // Transfer 256 16-bit values, a uint16_t at a time, into your buffer from I/O port 0x1F0. (In assembler, REP INSW works well for this.)
        for (int i = 0;i < 256;i++)
            target[i] = inw(0x1F0);
        target += 256;
    }
}

// Write sectors to the primary ATA device using 28bit PIO method 
// 
// LBA: 0-based Linear Block Address, 28bit LBA shall be between 0 to 0x0FFFFFFF
// sector_count: How many sectors you want to write
// source: a buffer whose length is sector_count*512 bytes 
//
void write_sectors_ATA_28bit_PIO(bool slave, uint32_t LBA, uint8_t sector_count, uint16_t* source) {
    ATA_wait_BSY();
    outb(0x1F6, 0xE0 | (slave << 4) | ((LBA >> 24) & 0xF));
    outb(0x1F2, sector_count);
    outb(0x1F3, (uint8_t)LBA);
    outb(0x1F4, (uint8_t)(LBA >> 8));
    outb(0x1F5, (uint8_t)(LBA >> 16));
    // To write sectors in 28 bit PIO mode, send command "WRITE SECTORS" (0x30) to the Command port
    outb(0x1F7, 0x30); //Send the write command

    for (int j = 0;j < sector_count;j++) {
        ATA_wait_BSY();
        ATA_wait_DRQ();
        for (int i = 0;i < 256;i++) {
            // Do not use REP OUTSW to transfer data. There must be a tiny delay between each OUTSW output uint16_t. A jmp $+2 size of delay
            outw(0x1F0, source[i]);
            io_wait();
        }
    }

    // Make sure to do a Cache Flush (ATA command 0xE7) after each write command completes.
    outb(0x1F7, 0xE7);
}

// Execute ATA PIO IDENTIFY command
// Ref: https://wiki.osdev.org/ATA_PIO_Mode#IDENTIFY_command
// 
// target: a memory location to store the returned 512 bytes structure
// 
// return: zero = success, otherwise failed
// 
int8_t ATA_Identify(bool slave, uint16_t* target) {
    ATA_wait_BSY();
    // select a target drive by sending 0xA0 for the master drive, or 0xB0 for the slave, to the "drive select" IO port (0x1F6)
    outb(0x1F6, 0xA0 | (slave << 4));
    // Then set the Sectorcount, LBAlo, LBAmid, and LBAhi IO ports to 0 (port 0x1F2 to 0x1F5)
    outb(0x1F2, 0);
    outb(0x1F3, 0);
    outb(0x1F4, 0);
    outb(0x1F5, 0);
    // Then send the IDENTIFY command (0xEC) to the Command IO port (0x1F7
    outb(0x1F7, 0xEC);

    // Then read the Status port (0x1F7) again. If the value read is 0, the drive does not exist.
    ATA_delay_400ns();
    uint8_t status = inb(0x1F7);
    if (status == 0) return -1;
    // For any other value: poll the Status port (0x1F7) until bit 7 (BSY, value = 0x80) clears.
    // Because of some ATAPI drives that do not follow spec, at this point you need to check the LBAmid and LBAhi ports (0x1F4 and 0x1F5) 
    // to see if they are non-zero. If so, the drive is not ATA, and you should stop polling.
    while (inb(0x1F7) & STATUS_BSY) {
        if (inb(0x1F4) != 0 || inb(0x1F5) != 0) return -2;
    };

    // Otherwise, continue polling one of the Status ports until bit 3 (DRQ, value = 8) sets, or until bit 0 (ERR, value = 1) sets.
    ATA_wait_DRQ();

    // At that point, if ERR is clear, the data is ready to read from the Data port (0x1F0). Read 256 16-bit values, and store them.
    if ((inb(0x1F7) & STATUS_ERR)) return 3;
    for (int i = 0;i < 256;i++) {
        target[i] = inw(0x1F0);
    }
    return 0;
}

// Get count of all sectors available to address using a 28bit LBA
//
// return: number of 28bit LBA available
int32_t get_total_28bit_sectors(bool slave) {
    uint16_t identifier[256];
    int8_t ret = ATA_Identify(slave, identifier);
    if (ret != 0) {
        return ret;
    }
    // uint16_t 60 & 61 taken as a uint32_t contain the total number of 28 bit LBA addressable sectors on the drive. (If non-zero, the drive supports LBA28.)
    // uint16_t 100 through 103 taken as a uint64_t contain the total number of 48 bit addressable sectors on the drive. (Probably also proof that LBA48 is supported.)
    uint32_t max_sector_28bit_lba = (((uint32_t)identifier[61]) << 16) + identifier[60];
    return (int32_t) max_sector_28bit_lba;
}


// Nedd to add 400ns delays before all the status registers are up to date
// https://wiki.osdev.org/ATA_PIO_Mode#400ns_delays
static void ATA_delay_400ns() {
    inb(0x1F7);
    inb(0x1F7);
    inb(0x1F7);
    inb(0x1F7);
}

// How to poll (waiting for the drive to be ready to transfer data): 
//  Read the Regular Status port until bit 7 (BSY, value = 0x80) clears, 
//      and bit 3 (DRQ, value = 8) sets 
//  -- or until bit 0 (ERR, value = 1) or bit 5 (DF, value = 0x20) sets. 
//  If neither error bit is set, the device is ready right then.
static void ATA_wait_BSY() {
    //Wait for BSY to be 0
    while (inb(0x1F7) & STATUS_BSY);
}
static void ATA_wait_DRQ() {
    //Wait fot DRQ or ERR to be 1
    while (!(inb(0x1F7) & (STATUS_RDY | STATUS_ERR)));
}
