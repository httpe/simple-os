#include <kernel/block_io.h>
#include <stddef.h>
#include <kernel/ata.h>
#include <kernel/panic.h>
#include <kernel/heap.h>

static uint32_t next_block_dev_id = 1;

#define MAX_STORAGE_DEV_COUNT 8

typedef struct ata_storage_info {
    bool is_slave;
} ata_storage_info;

static block_storage storage_list[MAX_STORAGE_DEV_COUNT];



static int64_t read_blocks_ata(block_storage* storage, void* buff,  uint32_t LBA, uint32_t block_count)
{
    ata_storage_info* info = (ata_storage_info*) storage->internal_info;
    PANIC_ASSERT(storage->block_size == 512);
    if(LBA >= storage->block_count || LBA + block_count >= storage->block_count) {
        return -1;
    }
    read_sectors_ATA_PIO(info->is_slave, buff, LBA, block_count);
    return 512 * block_count;
}

static int64_t write_blocks_ata(block_storage* storage, uint32_t LBA, uint32_t block_count, const void* buff)
{
    ata_storage_info* info = (ata_storage_info*) storage->internal_info;
    PANIC_ASSERT(storage->block_size == 512);
    if(LBA >= storage->block_count || LBA + block_count >= storage->block_count) {
        return -1;
    }
    write_sectors_ATA_PIO(info->is_slave, buff, LBA, block_count);
    return 512 * block_count;
}

static void add_block_storage(block_storage* storage)
{
    storage->device_id = next_block_dev_id++;
    for(uint32_t i=0; i<MAX_STORAGE_DEV_COUNT; i++) {
        if(storage_list[i].device_id == 0) {
            //id == 0 means unused slot
            storage_list[i] = *storage;
            return;
        }
    }
    PANIC("Too many block storage devices!");
}

block_storage* get_block_storage(uint32_t device_id)
{
    for(uint32_t i=0; i<MAX_STORAGE_DEV_COUNT; i++) {
        if(storage_list[i].device_id == device_id) {
            return &storage_list[i];
        }
    }
    return NULL;
}

void initialize_block_storage()
{
    // Add master/slave IDE devices
    
    int32_t total_block_count = get_total_28bit_sectors(false);
    PANIC_ASSERT(total_block_count > 0);

    ata_storage_info* master_info = kmalloc(sizeof(ata_storage_info));
    master_info->is_slave = false;
    block_storage master_storage = (block_storage) {
        .type=BLK_STORAGE_TYP_ATA_HARD_DRIVE, 
        .block_size=512, 
        .block_count=total_block_count, 
        .read_blocks=read_blocks_ata,
        .write_blocks=write_blocks_ata,
        .internal_info=master_info
    };
    add_block_storage(&master_storage);
    assert(master_storage.device_id == IDE_MASTER_DRIVE);

    int32_t slave_total_block_count = get_total_28bit_sectors(true);
    if(slave_total_block_count > 0) {
        // if slave IDE drive exist
        ata_storage_info* slave_info = kmalloc(sizeof(ata_storage_info));
        slave_info->is_slave = true;
        block_storage slave_storage = (block_storage) {
            .type=BLK_STORAGE_TYP_ATA_HARD_DRIVE, 
            .block_size=512, 
            .block_count=slave_total_block_count, 
            .read_blocks=read_blocks_ata,
            .write_blocks=write_blocks_ata,
            .internal_info=slave_info
        };
        add_block_storage(&slave_storage);
        assert(slave_storage.device_id == IDE_SLAVE_DRIVE);
    }

}