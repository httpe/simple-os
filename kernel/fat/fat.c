#include <stdlib.h>
#include <string.h>
#include <common.h>
#include <assert.h>
#include <stddef.h>

#include <kernel/errno.h>
#include <kernel/stat.h>
#include <kernel/fat.h>

#define HAS_ATTR(file,attr) (((file)&(attr)) == (attr))

// Source: https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
static uint8_t lfn_checksum(const uint8_t *pFCBName)
{
   int i;
   uint8_t sum = 0;

   for (i = 11; i; i--)
      sum = ((sum & 1) << 7) + (sum >> 1) + *pFCBName++;

   return sum;
}

// Source: Xv6/fs.c (skipelem)
// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   split_path("a/bb/c", name) = "bb/c", setting name = "a"
//   split_path("///a//bb", name) = "bb", setting name = "a"
//   split_path("a", name) = "", setting name = "a"
//   split_path("", name) = split_path("////", name) = NULL
//   split_path("/a", name) = "", setting name = "a"
// Edge Cases:
//   split_path("/a") = "", name = "a"
//   split_path("/") = NULL, name not touched
//   split_path("/a/bb") = "bb", name = "a"
//   split_path("bb") = "", name = "bb"
//   split_path("bb/cc") = "cc", name = "bb"
//
static char* split_path(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return NULL;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= FAT32_FILENAME_SIZE)
    memmove(name, s, FAT32_FILENAME_SIZE);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

static fat_cluster_status fat32_interpret_fat_entry(uint32_t entry)
{
    entry = entry & 0x0FFFFFFF;

    // Cluster value ref: https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system#Cluster_values
    if(entry == 0x00000000) {
        return FAT_CLUSTER_FREE;
    }
    if(entry >= 0x00000002 && entry <= 0x0FFFFFEF) {
        return FAT_CLUSTER_USED;
    }
    if(entry == 0x0FFFFFF7){
        return FAT_CLUSTER_BAD;
    }
    if(entry >= 0x0FFFFFF8) {
        // End of cluster-chain (EOC)
        return FAT_CLUSTER_EOC;
    }
    return FAT_CLUSTER_RESERVED;
}

static int fat32_get_meta(fat32_meta* meta)
{
    block_storage* storage = meta->storage;
    
    //TODO: Recognize MBR partitions
    uint sectors_to_read = 1 + (sizeof(fat32_bootsector) - 1) / storage->block_size;
    uint8_t* buff = malloc(sectors_to_read*storage->block_size);
    int64_t bytes_read = storage->read_blocks(storage, buff, 0, sectors_to_read);
    if(bytes_read != storage->block_size*sectors_to_read) {
        goto free_buff;
    }
    uint partition_start_lba = 0;
    mbr_partition_table_entry* partition_table = (mbr_partition_table_entry*) &buff[0x1BE];
    for(int i=0;i<4;i++) {
        if(partition_table[i].driver_attributes == 0x80 && partition_table[i].partition_type == 0x0C) {
            if(partition_table[i].LBA_partition_start > 0 && partition_table[i].partition_sector_count>0) {
                // an Active & FAT32 partition found
                partition_start_lba = partition_table[i].LBA_partition_start;
                memset(buff, 0, sectors_to_read*storage->block_size);
                bytes_read = storage->read_blocks(storage, buff, partition_start_lba, sectors_to_read);
                if(bytes_read != storage->block_size*sectors_to_read) {
                    goto free_buff;
                }
                break;
            }
        }
    }

    meta->bootsector = malloc(sizeof(fat32_bootsector));
    memmove(meta->bootsector, buff, sizeof(*meta->bootsector));
    // Sanity check
    uint good = 1;
    good = good & (meta->bootsector->mbr_signature == 0xAA55); // ensure MBR magic number
    good = good & (meta->bootsector->bytes_per_sector ==  storage->block_size); // ensure sector size
    good = good & (meta->bootsector->root_entry_count == 0); // ensure is FAT32 not FAT12/16
    good = good & (meta->bootsector->boot_signature == 0x29); // ensure FAT signature
    good = good & (meta->bootsector->hidden_sector_count == partition_start_lba); // make sure it conforms the partition table
    // we assume cluster number is in the range of int32_t, check it here
    good = good & (meta->bootsector->total_sectors_32 / meta->bootsector->sectors_per_cluster < 0x7FFFFFFF); 
    if(!good){
        goto free_bootsector;
    }
    // Read FS Info
    sectors_to_read = 1 + (sizeof(fat32_fsinfo) - 1) / storage->block_size;
    bytes_read = storage->read_blocks(storage, buff, meta->bootsector->hidden_sector_count + meta->bootsector->fs_info_sector, sectors_to_read);
    if(bytes_read != storage->block_size*sectors_to_read) {
        return -1;
    }
    meta->fs_info = malloc(sizeof(fat32_fsinfo));
    memmove(meta->fs_info, buff, sizeof(*meta->fs_info));
    good = good & (meta->fs_info->lead_signature == 0x41615252); // check FS_Info magic number
    good = good & (meta->fs_info->structure_signature == 0x61417272);
    good = good & (meta->fs_info->trailing_signature ==  0xAA550000);
    if(!good){
        goto free_fs_info;
    }
    // Read FAT
    if(meta->bootsector->table_count == 0) {
        goto free_fs_info;
    }
    uint fat_byte_size = meta->bootsector->table_sector_size_32 * meta->bootsector->bytes_per_sector;
    meta->fat = malloc(fat_byte_size);
    bytes_read = storage->read_blocks(storage, (uint8_t*) meta->fat, meta->bootsector->hidden_sector_count +  meta->bootsector->reserved_sector_count, meta->bootsector->table_sector_size_32);
    if(bytes_read != fat_byte_size) {
        goto free_fat;
    }
    good = good & ((meta->fat[0] & 0x0FFFFFFF) >= 0x0FFFFFF0) & ((meta->fat[0] & 0x0FFFFFFF) <= 0x0FFFFFFF); // check cluster 0 (FAT ID)
    good = good & ((meta->fat[1] & 0x0FFFFFFF) == 0x0FFFFFFF); // check cluster 1 (End of Cluster Mark)
    if(!good){
        goto free_fat;
    }
    // Ensure all FAT are the same
    uint32_t* alternative_fat = malloc(fat_byte_size);
    for(uint32_t fat_idx = 1; fat_idx < meta->bootsector->table_count; fat_idx++){
        bytes_read = storage->read_blocks(storage, (uint8_t*) alternative_fat, meta->bootsector->hidden_sector_count + meta->bootsector->reserved_sector_count + fat_idx*meta->bootsector->table_sector_size_32, meta->bootsector->table_sector_size_32);
        if(bytes_read != fat_byte_size) {
            goto free_alternative_fat;
        }
        if(memcmp(alternative_fat, meta->fat, fat_byte_size) != 0) {
            goto free_alternative_fat;
        }
    }

    return 0;

free_alternative_fat:
    free(alternative_fat);
free_fat:
    free(meta->fat);
free_fs_info:
    free(meta->fs_info);
free_bootsector:
    free(meta->bootsector);
free_buff:
    free(buff);

return -1;


}

static fat_cluster_status fat32_get_cluster_info(fat32_meta* meta, uint cluster_number, fat_cluster* cluster)
{
    // First&second cluster are reserved for FAT ID and End of Cluster Mark 
    if(cluster_number <= 1) {
        return FAT_CLUSTER_RESERVED;
    }
    
    fat_cluster_status status = fat32_interpret_fat_entry(meta->fat[cluster_number]);
    if(status == FAT_CLUSTER_USED) {
        cluster->next = meta->fat[cluster_number] & 0x0FFFFFFF;
    } else {
        cluster->next = 0;
    }
    cluster->curr = cluster_number;

    return status;
}

static uint count_clusters(fat32_meta* meta, uint cluster_number)
{
    fat_cluster cluster;
    cluster.next = cluster_number;
    uint total_cluster_count = 0;
    while(1) {
        fat32_get_cluster_info(meta, cluster.next, &cluster);
        total_cluster_count++;
        if(cluster.next == 0) {
            return total_cluster_count;
        }
    }
}

// Get cluster number by indexing into a cluster chain, negative index means counting from EOC
static uint fat32_index_cluster_chain(fat32_meta* meta, uint cluster_number, int index)
{
    if(cluster_number == 0) {
        return 0;
    }

    if(index < 0) {
        uint cluster_count = count_clusters(meta, cluster_number);
        index = cluster_count + index;
    }
    if(index < 0) {
        return 0;
    }
    if(index == 0) {
        return cluster_number;
    }
    fat_cluster cluster = {.next = cluster_number};
    while(index > 0) {
        if(cluster.next == 0) {
            return 0;
        }
        fat32_get_cluster_info(meta, cluster.next, &cluster);
        index--;
    }
    return cluster.next;
}

static int fat32_write_meta(fat32_meta* meta, fat32_meta* new_meta)
{
    uint fat_byte_size = meta->bootsector->table_sector_size_32*meta->bootsector->bytes_per_sector;
    // Write new FAT to main FAT and backups
    uint fat_idx;
    uint bytes_written;
    for(fat_idx = 0; fat_idx < meta->bootsector->table_count; fat_idx++){
        uint lba = meta->bootsector->hidden_sector_count + meta->bootsector->reserved_sector_count + fat_idx*meta->bootsector->table_sector_size_32;
        bytes_written = meta->storage->write_blocks(meta->storage, lba, meta->bootsector->table_sector_size_32, (uint8_t*) new_meta->fat);
        if(bytes_written != fat_byte_size) {
            break;
        }
    }
    // If failed
    if(fat_idx != meta->bootsector->table_count) {
        // FAT corrupted!
        // Try restore back to the original FAT
        for(uint fat_idx_recover = 0; fat_idx_recover <= fat_idx; fat_idx_recover++){
            uint lba = meta->bootsector->hidden_sector_count + meta->bootsector->reserved_sector_count + fat_idx_recover*meta->bootsector->table_sector_size_32;
            bytes_written = meta->storage->write_blocks(meta->storage, lba, meta->bootsector->table_sector_size_32, (uint8_t*) meta->fat);
            // If recover attempt failed, panic
            assert(bytes_written != fat_byte_size);
        }
        return -1;
    }
    
    // FS Info is information only, so no rollback and return 0 even if error
    uint fsinfo_sector_size = 1 + (sizeof(fat32_fsinfo) - 1) / new_meta->storage->block_size;
    bytes_written = new_meta->storage->write_blocks(new_meta->storage, meta->bootsector->hidden_sector_count + new_meta->bootsector->fs_info_sector, fsinfo_sector_size, (uint8_t*) new_meta->fs_info);
    // if(bytes_written != new_meta->storage->block_size*fsinfo_sector_size) {
    //     return -1;
    // }


    return 0;
}

static void fat32_copy_meta(fat32_meta* new_meta, fat32_meta* meta)
{

    uint fat_byte_size = meta->bootsector->table_sector_size_32*meta->bootsector->bytes_per_sector;
    if(new_meta->fat == NULL) {
        new_meta->fat = malloc(fat_byte_size);
    }
    memmove(new_meta->fat, meta->fat, fat_byte_size);

    if(new_meta->fs_info == NULL) {
        new_meta->fs_info = malloc(sizeof(fat32_fsinfo));
    }
    memmove(new_meta->fs_info, meta->fs_info, sizeof(fat32_fsinfo));

    if(new_meta->bootsector == NULL) {
        new_meta->bootsector = malloc(sizeof(fat32_bootsector));
    }
    memmove(new_meta->bootsector, meta->bootsector, sizeof(*meta->bootsector));

    new_meta->storage = meta->storage;
}

static void fat32_free_meta(fat32_meta* meta)
{
    free(meta->bootsector);
    meta->bootsector = NULL;
    free(meta->fs_info);
    meta->fs_info = NULL;
    free(meta->fat);
    meta->fat = NULL;
    meta->storage = NULL;
}

// Return: Cluster number of the first newly allocated cluster
static uint fat32_allocate_cluster(fat32_meta* meta, uint prev_cluster_number, uint cluster_count_to_allocate)
{
    uint cluster_number = meta->fs_info->next_free_cluster;
    if(cluster_number == 0xFFFFFFFF) {
        cluster_number = 2;
    }
    uint cluster_number_first_tried = cluster_number;
    uint max_cluster_number = meta->bootsector->table_sector_size_32*meta->bootsector->bytes_per_sector / 4 - 1;
    uint allocated = 0, first_new_cluster_number = 0;
    
    fat32_meta new_meta = {0};
    fat32_copy_meta(&new_meta, meta);
    
    while(allocated < cluster_count_to_allocate) {
        fat_cluster_status status = fat32_interpret_fat_entry(new_meta.fat[cluster_number]);
        if(status == FAT_CLUSTER_FREE) {
            if(prev_cluster_number != 0) {
                fat_cluster_status prev_status = fat32_interpret_fat_entry(new_meta.fat[prev_cluster_number]);
                assert(prev_status == FAT_CLUSTER_EOC);
                new_meta.fat[prev_cluster_number] = (new_meta.fat[prev_cluster_number] & 0xF0000000) | (cluster_number & 0x0FFFFFFF);
            }
            new_meta.fat[cluster_number] = (new_meta.fat[cluster_number] & 0xF0000000) | (FAT_CLUSTER_EOC & 0x0FFFFFFF);
            prev_cluster_number = cluster_number;
            if(allocated == 0) {
                first_new_cluster_number = cluster_number;
            }
            allocated++;
        }
        if(cluster_number == max_cluster_number) {
            cluster_number = 2;
        } else {
            cluster_number++;
        }
        if(cluster_number == cluster_number_first_tried && allocated < cluster_count_to_allocate) {
            // all FAT entries tested, no free entry, disk is full
            fat32_free_meta(&new_meta);
            return 0;
        }
    }

    if(meta->fs_info->free_cluster_count != 0xFFFFFFFF) {
        meta->fs_info->free_cluster_count -= allocated;
    }
    meta->fs_info->next_free_cluster = cluster_number; // not a free cluster, but a good place to start looking for one

    int res = fat32_write_meta(meta, &new_meta);
    if(res == 0) {
        // Update memory cache
        fat32_copy_meta(meta, &new_meta);
        fat32_free_meta(&new_meta);

        return first_new_cluster_number;
    } else {
        fat32_free_meta(&new_meta);
        return 0;
    }
}


// cluster_count_to_free = 0 means free to the end of the chain
static int fat32_free_cluster(fat32_meta* meta, uint prev_cluster_number, uint cluster_number, uint cluster_count_to_free)
{
    if(cluster_number == 0) {
        return 0;
    }

    fat32_meta new_meta = {0};
    fat32_copy_meta(&new_meta, meta);
    
    uint cluster_freed = 0;
    fat_cluster cluster = {.next = cluster_number};
    while(cluster.next && (cluster_freed < cluster_count_to_free || cluster_count_to_free == 0)) {
        fat_cluster_status status = fat32_get_cluster_info(meta, cluster.next, &cluster);
        assert(status == FAT_CLUSTER_USED || status == FAT_CLUSTER_EOC);
        // set as free cluster
        new_meta.fat[cluster.curr] = (new_meta.fat[cluster.curr] & 0xF0000000) | (FAT_CLUSTER_FREE & 0x0FFFFFFF);
        if(prev_cluster_number != 0) {
            if(cluster.next != 0) {
                // if removing cluster in the middle of the chain, connect prev and next cluster
                new_meta.fat[prev_cluster_number] = (new_meta.fat[prev_cluster_number] & 0xF0000000) | (cluster.next & 0x0FFFFFFF);
            } else {
                new_meta.fat[prev_cluster_number] = (new_meta.fat[prev_cluster_number] & 0xF0000000) | (FAT_CLUSTER_EOC & 0x0FFFFFFF);
            }   
        }
        cluster_freed++;
    }

    if(meta->fs_info->free_cluster_count != 0xFFFFFFFF) {
        meta->fs_info->free_cluster_count += cluster_freed;
    }

    int res = fat32_write_meta(meta, &new_meta);
    if(res == 0) {
        // Update memory cache
        fat32_copy_meta(meta, &new_meta);
        fat32_free_meta(&new_meta);

        return 0;
    } else {
        fat32_free_meta(&new_meta);
        return -EIO;
    }

}

static int64_t fat32_read_clusters(fat32_meta* meta, uint cluster_number, uint clusters_to_read, uint8_t* buff) 
{
    assert(cluster_number >= 2);
    uint cluster_byte_size = meta->bootsector->bytes_per_sector*meta->bootsector->sectors_per_cluster;

    fat_cluster cluster = {.next = cluster_number};
    int64_t total_bytes_read = 0;
    for(uint i=0; i < clusters_to_read; i++) {
        assert(cluster.next != 0);
        fat_cluster_status status = fat32_get_cluster_info(meta, cluster.next, &cluster);
        assert(status == FAT_CLUSTER_USED || (status == FAT_CLUSTER_EOC && i == clusters_to_read-1));
        // the cluster 0 and 1 are not of size sectors_per_cluster
        uint lba = meta->bootsector->hidden_sector_count + meta->bootsector->reserved_sector_count + meta->bootsector->table_sector_size_32*meta->bootsector->table_count + (cluster.curr-2)*meta->bootsector->sectors_per_cluster;
        int64_t bytes_read = meta->storage->read_blocks(meta->storage, buff, lba, meta->bootsector->sectors_per_cluster);
        if(bytes_read < 0) {
            return bytes_read;
        }
        total_bytes_read += bytes_read;
        buff += cluster_byte_size;
        lba += meta->bootsector->sectors_per_cluster;
    }

    return total_bytes_read;
}

static int64_t fat32_write_clusters(fat32_meta* meta, uint cluster_number, uint clusters_to_write, uint8_t* buff)
{
    assert(cluster_number >= 2);
    uint cluster_byte_size = meta->bootsector->bytes_per_sector*meta->bootsector->sectors_per_cluster;

    fat_cluster cluster = {.next = cluster_number};
    int64_t total_bytes_written = 0;
    for(uint i=0; i < clusters_to_write; i++) {
        assert(cluster.next != 0);
        fat_cluster_status status = fat32_get_cluster_info(meta, cluster.next, &cluster);
        assert(status == FAT_CLUSTER_USED || (status == FAT_CLUSTER_EOC && i == clusters_to_write-1));
        // the cluster 0 and 1 are not of size sectors_per_cluster
        uint lba = meta->bootsector->hidden_sector_count + meta->bootsector->reserved_sector_count + meta->bootsector->table_sector_size_32*meta->bootsector->table_count + (cluster.curr-2)*meta->bootsector->sectors_per_cluster;
        int64_t bytes_written = meta->storage->write_blocks(meta->storage, lba, meta->bootsector->sectors_per_cluster, buff);
        if(bytes_written < 0) {
            return bytes_written;
        }
        total_bytes_written += bytes_written;
        buff += cluster_byte_size;
        lba += meta->bootsector->sectors_per_cluster;
    }
    return total_bytes_written;
}

// trim leading and trailing spaces and trailing periods
static void trim_file_name(char* str)
{
    if(str == NULL) {
        return;
    }
    uint start = 0;
    while(str[start] == ' ' && str[start] != 0) {
        start++;
    }
    if(str[start] == 0) {
        // str is all spaces
        str[0] = 0;
        return;
    }
    uint end = strlen(str) - 1;
    while((str[end] == ' ' || str[end] == '.') && end > start) {
        end--;
    }
    memmove(str, &str[start], end - start + 1);
    str[end - start + 1] = 0;
}

// Convert the 8.3 filename entry to its displayed version
static void fat_standardize_short_name(char* filename, fat32_direntry_short* short_entry)
{
    memmove(filename,short_entry->name, FAT_SHORT_NAME_LEN);
    if(filename[0] == '.') {
        // dot entry shall have name '.' or '..'
        if(filename[1] == '.') {
            filename[2] = 0;
        } else {
            filename[1] = 0;
        }
        return;
    }
    filename[FAT_SHORT_NAME_LEN] = 0;
    trim_file_name((char*)filename);
    uint name_len = strlen((char*)filename);
    filename[name_len] = '.';
    memmove(&filename[name_len+1],  short_entry->ext, FAT_SHORT_EXT_LEN);
    filename[name_len+1+FAT_SHORT_EXT_LEN] = 0;
    trim_file_name((char*)filename);
    if(filename[0] == 0x05) {
        // If DIR_Name[0] == 0x05, then the actual file name character for this byte is 0xE5
       filename[0] = 0xE5;
    }
}

static void fat32_reset_dir_iterator(fat_dir_iterator* iter)
{
    iter->current_dir_entry_idx = 0;
}

static void fat_free_dir_iterator(fat_dir_iterator* iter)
{
    free(iter->dir_entries);
    iter->dir_entries = NULL;
    iter->dir_entry_count = 0;
    iter->entry_per_cluster = 0;
    fat32_reset_dir_iterator(iter);
}

static fat_iterate_dir_status fat32_iterate_dir(fat32_meta* meta, fat_dir_iterator* iter, fat32_file_entry* file_entry)
{
    if(iter->dir_entries == NULL) {
        // if buff is null, read the whole dir into memory
        uint cluster_byte_size = meta->bootsector->sectors_per_cluster * meta->bootsector->bytes_per_sector;
        uint dir_total_cluster_count = count_clusters(meta, iter->first_cluster);
        iter->dir_entries = malloc(dir_total_cluster_count*cluster_byte_size);
        int64_t read_res = fat32_read_clusters(meta, iter->first_cluster, dir_total_cluster_count, (uint8_t*) iter->dir_entries);
        if(read_res < 0) {
            fat_free_dir_iterator(iter);
            return FAT_DIR_ITER_ERROR;
        }
        iter->entry_per_cluster = cluster_byte_size/sizeof(fat32_direntry_t);
        iter->dir_entry_count = dir_total_cluster_count*iter->entry_per_cluster;
        fat32_reset_dir_iterator(iter);
    }

    memset(file_entry, 0, sizeof(*file_entry));
    file_entry->dir_cluster = iter->first_cluster;
	uint lfn_entry_buffered = 0;
	uint8_t last_lfn_checksum = 0;

    while(1)
    {
        if(iter->current_dir_entry_idx >= iter->dir_entry_count) {
            return FAT_DIR_ITER_NO_MORE_ENTRY;
        }

        fat32_direntry_t* entry = &iter->dir_entries[iter->current_dir_entry_idx];
        // Algo Ref: https://wiki.osdev.org/FAT#Reading_Directories
        if(entry->short_entry.attr == FAT_ATTR_LFN && entry->short_entry.nameext[0] != 0xE5){
            // Is this entry a long file name entry? If the 11'th byte of the entry equals 0x0F, then it is a long file name entry. Otherwise, it is not.
            // Read the portion of the long filename into a temporary buffer. Goto 8.
            if((entry->long_entry.seq & 0x40) == 0x40) {
                // first LFN entry
                // if some entries are already buffered, they will be abandoned
                memset(file_entry->filename, 0, sizeof(file_entry->filename));
                void* end_of_filename = ((void*) file_entry->filename) + sizeof(file_entry->filename);
                memmove(end_of_filename-=sizeof(entry->long_entry.name3), entry->long_entry.name3, sizeof(entry->long_entry.name3));
                memmove(end_of_filename-=sizeof(entry->long_entry.name2), entry->long_entry.name2, sizeof(entry->long_entry.name2));
                memmove(end_of_filename-=sizeof(entry->long_entry.name1), entry->long_entry.name1, sizeof(entry->long_entry.name1));
                file_entry->first_dir_entry_idx = iter->current_dir_entry_idx;
                last_lfn_checksum = entry->long_entry.csum;
                lfn_entry_buffered = 1;
            } else if(last_lfn_checksum != entry->long_entry.csum) {
                // skip this LFN entry if checksum doesn't match the last checksum
                lfn_entry_buffered = lfn_entry_buffered;
            } else {
                // not first LFN and checksum is consistent
                void* offset = ((void*) file_entry->filename) + sizeof(file_entry->filename) - FAT32_USC2_FILE_NAME_LEN_PER_LFN*2*(lfn_entry_buffered);
                memmove(offset-=sizeof(entry->long_entry.name3), entry->long_entry.name3, sizeof(entry->long_entry.name3));
                memmove(offset-=sizeof(entry->long_entry.name2), entry->long_entry.name2, sizeof(entry->long_entry.name2));
                memmove(offset-=sizeof(entry->long_entry.name1), entry->long_entry.name1, sizeof(entry->long_entry.name1));
                lfn_entry_buffered++;
            }
            file_entry->dir_entry_count++;
            iter->current_dir_entry_idx++;
            continue;
        } else {
            // Parse the data for this entry using the table from further up on this page. It would be a good idea to save the data for later. Possibly in a virtual file system structure. goto 6
            file_entry->direntry = iter->dir_entries[iter->current_dir_entry_idx].short_entry;
            file_entry->dir_entry_count++;
            // Is there a long file name in the temporary buffer? Yes, goto 7. No, goto 8
            // Apply the long file name to the entry that you just read and clear the temporary buffer. goto 8
            if(lfn_entry_buffered > 0) {
                // We do not support USC-2 UNICODE character, any non US-ASCII character will be replaced by '_'
                // as per Microsoft's documentation "Microsoft Extensible Firmware Initiative FAT32 File System Specification" 
                // https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/fatgen103.doc
                uint lfn_name_byte_len =  FAT32_USC2_FILE_NAME_LEN_PER_LFN*2*lfn_entry_buffered;
                char* start_of_filename = &file_entry->filename[sizeof(file_entry->filename)] - lfn_name_byte_len;
                for(uint i=0; i<lfn_name_byte_len/2; i++) {
                    char usc2_first = start_of_filename[i*2];
                    char usc2_second = start_of_filename[i*2+1];
                    // Unicode (and UCS-2) is compatible with 7-bit ASCII / US-ASCII
                    if(usc2_first < 0 || usc2_second != 0) {
                        // if not US-ASCII
                        file_entry->filename[i] = '_';
                    } else {
                        file_entry->filename[i] = usc2_first;
                    }
                }
                file_entry->filename[lfn_name_byte_len] = 0;
                trim_file_name((char*)file_entry->filename);
            } else {
                // if not LFN buffered, use the 8.3 short name
                file_entry->first_dir_entry_idx = iter->current_dir_entry_idx;
                fat_standardize_short_name((char*) file_entry->filename, &file_entry->direntry);
            }
            
            // Increment pointers and/or counters and check the next entry. (goto number 1)
            iter->current_dir_entry_idx++;

            if(entry->short_entry.nameext[0] == 0) {
                // If the first byte of the entry is equal to 0 then there are no more files/directories in this directory. FirstByte==0, finish.
                return FAT_DIR_ITER_FREE_ENTRY;
            }
            if(entry->short_entry.nameext[0] == 0x2E) {
                // Entry for either "." or ".." (dot is not allowed otherwise in short name)
                return FAT_DIR_ITER_DOT_ENTRY;
            }
            if(entry->short_entry.nameext[0] == 0xE5) {
                // If the first byte of the entry is equal to 0xE5 then the entry is unused. FirstByte==0xE5, goto 8
                return FAT_DIR_ITER_DELETED;
            }
            if(lfn_entry_buffered > 0 && last_lfn_checksum != lfn_checksum(file_entry->direntry.nameext)) {
                // invalid LFN entry, see as error
                return FAT_DIR_ITER_ERROR;
            }
            return FAT_DIR_ITER_VALID_ENTRY;
        }
    }


}

static fat_resolve_path_status fat32_dir_lookup(fat32_meta* meta, fat_dir_iterator* iter, const char *filename, fat32_file_entry* file_entry)
{
    char shortname[FAT_SHORT_NAME_LEN + FAT_SHORT_EXT_LEN+1];
    fat32_reset_dir_iterator(iter);
    while(1) {
        fat_iterate_dir_status iter_status = fat32_iterate_dir(meta, iter, file_entry);
        if(iter_status == FAT_DIR_ITER_ERROR) {
            // Any error will discard all info we got
            return FAT_PATH_RESOLVE_ERROR;
        }
        if(iter_status == FAT_DIR_ITER_DELETED || iter_status == FAT_DIR_ITER_DOT_ENTRY) {
            continue;
        }
        if(iter_status == FAT_DIR_ITER_NO_MORE_ENTRY || iter_status == FAT_DIR_ITER_FREE_ENTRY) {
            return FAT_PATH_RESOLVE_NOT_FOUND;
        }
        assert(iter_status == FAT_DIR_ITER_VALID_ENTRY);
        // TODO: Case insensitive matching
        if(strcmp((char*) file_entry->filename, filename) == 0) {
            return FAT_PATH_RESOLVE_FOUND;
        }
        fat_standardize_short_name(shortname, &file_entry->direntry);
        if(strcmp((char*) shortname, filename) == 0) {
            return FAT_PATH_RESOLVE_FOUND;
        }
    }
}

static fat_resolve_path_status fat32_resolve_path(fat32_meta* meta, const char *path, fat32_file_entry* file_entry)
{

    char filename[FAT32_FILENAME_SIZE] = {0};
    char* remainding_path = split_path((char*) path, filename);

    fat_dir_iterator iter = {.first_cluster = meta->bootsector->root_cluster};

    if(strlen(path) == 0) {
        return FAT_PATH_RESOLVE_INVALID_PATH;
    }
    if(*path != '/') {
        return FAT_PATH_RESOLVE_INVALID_PATH;
    }
    if(remainding_path == NULL) {
        return FAT_PATH_RESOLVE_ROOT_DIR;
    }

    fat_resolve_path_status resolve_status;
    while(1) {
        resolve_status = fat32_dir_lookup(meta, &iter, filename, file_entry);
        if(resolve_status != FAT_PATH_RESOLVE_FOUND) {
            // if not found or error
            break;
        }
        if(remainding_path == NULL || *remainding_path==0) {
            // if is last part of the path
            break;
        } else {
            if(!HAS_ATTR(file_entry->direntry.attr,FAT_ATTR_DIRECTORY)) {
                // not a dir
                resolve_status = FAT_PATH_RESOLVE_INVALID_PATH;
                break;
            }
            fat_free_dir_iterator(&iter);
            iter.first_cluster = file_entry->direntry.cluster_lo + (file_entry->direntry.cluster_hi << 16);
            remainding_path = split_path(remainding_path, filename);
        }
    }

    fat_free_dir_iterator(&iter);

    if(resolve_status == FAT_PATH_RESOLVE_NOT_FOUND && !(remainding_path == NULL || *remainding_path==0)) {
        // if a middle part of the path is not found, return invalid path instead of not found 
        return FAT_PATH_RESOLVE_INVALID_PATH;
    }

    return resolve_status;
}


static date_time convert_datetime(uint16_t date_entry, uint16_t time_entry) {
	date_time time_info;
	time_info.tm_sec = (time_entry & 0x1F) << 1;
	time_info.tm_min = (time_entry & 0x7E0) >> 5;
	time_info.tm_hour = (time_entry & 0xF800) >> 11;
	time_info.tm_mday = date_entry & 0x1F;
	time_info.tm_mon = ((date_entry & 0x1E0) >> 5) - 1;
	time_info.tm_year = ((date_entry & 0xFE00) >> 9) + 80;
	return time_info;
}

void fat32_set_timestamp(uint16_t* date_entry, uint16_t* time_entry)
{
    date_time time_info = current_datetime();
    *time_entry = 0;
    *time_entry += (time_info.tm_sec >> 1);
    *time_entry += (time_info.tm_min << 5);
    *time_entry += (time_info.tm_hour << 11);
    *date_entry = 0;
    *date_entry += time_info.tm_mday;
    *date_entry += (time_info.tm_mon + 1) << 5;
    *date_entry += (time_info.tm_year - 80) << 9;
}


// Set short name according to the standardized filename
static int fat32_set_short_name(fat32_file_entry* file_entry)
{
    // TODO: Add checks for illegal characters
    int filename_len = (int) strlen((char*)file_entry->filename);
    assert(filename_len > 0);
    memset(file_entry->direntry.nameext, ' ', FAT_SHORT_NAME_LEN + FAT_SHORT_EXT_LEN);
    uint copied = 0;
    for(int i = 0; copied < FAT_SHORT_NAME_LEN && i < filename_len; i++) {
        if(file_entry->filename[i] == ' ') {
            continue;
        }
        if(copied == 0 && file_entry->filename[i] == '.') {
            continue;
        }
        if(file_entry->filename[i] == '.') {
            break;
        }
        file_entry->direntry.name[copied] = file_entry->filename[i];
        if(copied == 0 && file_entry->direntry.name[copied] == 0xE5) {
            file_entry->direntry.name[0] = 0x05;
        }
        copied++;
    }
    copied = 0;
    for(int i = filename_len - 1; i >= 0; i--) {
        // find last period
        if(file_entry->filename[i] == '.') {
            for(int j = i + 1; j < filename_len; j++) {
                if(file_entry->filename[j] != ' ' && file_entry->filename[j] != '.') {
                    file_entry->direntry.ext[copied] = file_entry->filename[j];
                    copied++;
                }
                if(copied == FAT_SHORT_EXT_LEN) {
                    break;
                }
            }
            break;
        }
    }
    return 0;
}

// Return: numeric tail appended for short name collision prevention
static int fat32_set_numeric_tail(fat32_meta* meta, fat_dir_iterator* iter, fat32_file_entry* file_entry)
{
    char shortname[FAT_SHORT_NAME_LEN + FAT_SHORT_EXT_LEN+1];
    fat32_file_entry existing_entry = {0};
    char buff[FAT_SHORT_NAME_LEN];

    // Assume we always need to add numeric tail here
    // Ref: http://elm-chan.org/fsw/ff/00index_e.html
    for (uint number_tail = 1; number_tail <= 999999; number_tail++) {
        uint seq = number_tail;
        int i=FAT_SHORT_NAME_LEN - 1;
        do {
            uint8_t c = (uint8_t)((seq % 16) + '0');
            if (c > '9') c += 7;
            buff[i--] = c;
            seq /= 16;
        } while(seq);
        buff[i] = '~';

        fat32_file_entry working_entry = *file_entry;
        /* Append the number to the SFN body */
        int j = 0;
        for (; j < i && working_entry.direntry.name[j] != ' '; j++);
        do {
            working_entry.direntry.name[j++] = (i < 8) ? buff[i++] : ' ';
        } while (j < 8);

        fat_standardize_short_name(shortname, &working_entry.direntry);
        fat_resolve_path_status status = fat32_dir_lookup(meta, iter, shortname, &existing_entry);
        if(status == FAT_PATH_RESOLVE_NOT_FOUND) {
            // if short name has no collision anymore
            memcpy(file_entry->direntry.name, working_entry.direntry.name, FAT_SHORT_NAME_LEN);
            return number_tail;
        }
    }
    return -1;
}

static int fat32_write_to_offset(fat32_meta* meta, uint first_cluster, uint offset, uint size, const uint8_t* buff)
{
    uint bytes_per_cluster = meta->bootsector->sectors_per_cluster*meta->bootsector->bytes_per_sector;
    uint cluster_start_writing = fat32_index_cluster_chain(meta, first_cluster, offset / bytes_per_cluster);
    uint clusters_to_write = 1 + ((offset + size - 1) / bytes_per_cluster) - (offset / bytes_per_cluster);
    fat_cluster cluster = {.next = cluster_start_writing};

    uint8_t* cluster_buff = malloc(bytes_per_cluster);
    uint start_offset_in_cluster, end_offset_in_cluster;
    for(uint i=0; i < clusters_to_write; i++) {
        assert(cluster.next != 0);
        fat_cluster_status cluster_status = fat32_get_cluster_info(meta, cluster.next, &cluster);
        assert(cluster_status == FAT_CLUSTER_USED || cluster_status == FAT_CLUSTER_EOC);
        int64_t read_res = fat32_read_clusters(meta, cluster.curr, 1, cluster_buff);
        if(read_res < 0) {
            free(cluster_buff);
            return read_res;
        }
        if(i == 0) {
            start_offset_in_cluster = offset % bytes_per_cluster;
        } else {
            start_offset_in_cluster = 0;
        }
        if(i == clusters_to_write - 1) {
            end_offset_in_cluster = (offset + size - 1) % bytes_per_cluster;
        } else {
            end_offset_in_cluster = bytes_per_cluster - 1;
        }
        memmove(cluster_buff + start_offset_in_cluster, buff, end_offset_in_cluster - start_offset_in_cluster + 1);
        int64_t write_res = fat32_write_clusters(meta, cluster.curr, 1, cluster_buff);
        if(write_res < 0) {
            free(cluster_buff);
            return write_res;
        }
        buff += end_offset_in_cluster - start_offset_in_cluster + 1;
    }
    free(cluster_buff);
    return 0;
}

// Return: dir entries added
static int fat32_add_file_entry(fat32_meta* meta, fat_dir_iterator* iter, fat32_file_entry* file_entry)
{
    uint cluster_byte_size = meta->bootsector->bytes_per_sector*meta->bootsector->sectors_per_cluster;
    uint dir_entry_per_cluster = cluster_byte_size / sizeof(fat32_direntry_t);

    // for sake of simplicity, will always save the file name as LFN entry, even if it fits into 8.3 format
    // filename are US-ASCII characters, each will take a USC-2 char space in LFN
    uint lfn_len = strlen(file_entry->filename);
    uint lfn_entry_needed = lfn_len == 0? 0: (lfn_len - 1)/FAT32_USC2_FILE_NAME_LEN_PER_LFN + 1;
    uint dir_entry_needed = lfn_entry_needed + 1;

    fat32_file_entry candidate_entry = {0};
    fat32_reset_dir_iterator(iter);

    // Find contagious free space for the entries
    uint free_entry_count = 0;
    int first_free_entry_idx = -1;
    while(1) {
        if(free_entry_count >= dir_entry_needed) {
            break;
        }
        fat_iterate_dir_status iter_status = fat32_iterate_dir(meta, iter, &candidate_entry);
        if(iter_status == FAT_DIR_ITER_ERROR) {
            return -EIO;
        }
        if(iter_status == FAT_DIR_ITER_VALID_ENTRY || iter_status == FAT_DIR_ITER_DOT_ENTRY) {
            free_entry_count = 0;
            first_free_entry_idx = -1;
            continue;
        }
        if(iter_status == FAT_DIR_ITER_DELETED || iter_status == FAT_DIR_ITER_FREE_ENTRY) {
            free_entry_count += candidate_entry.dir_entry_count;
            if(first_free_entry_idx < 0) {
                first_free_entry_idx = candidate_entry.first_dir_entry_idx;
            }
            continue;
        }
        assert(iter_status == FAT_DIR_ITER_NO_MORE_ENTRY);
        assert(free_entry_count < dir_entry_needed);
        // if reach the end of entries and still no enough space, allocate a new cluster
        uint clusters_to_alloc = (dir_entry_needed - free_entry_count - 1) / dir_entry_per_cluster + 1;
        uint first_new_cluster = fat32_allocate_cluster(meta, fat32_index_cluster_chain(meta, iter->first_cluster, -1), clusters_to_alloc);
        if(first_new_cluster == 0) {
            return -EIO;
        }
        // Fill zeros to the new clusters 
        uint8_t* zeros = malloc(clusters_to_alloc*cluster_byte_size);
        memset(zeros, 0, clusters_to_alloc*cluster_byte_size);
        int64_t write_res = fat32_write_clusters(meta, first_new_cluster, clusters_to_alloc, zeros);
        free(zeros);
        if(write_res < 0) {
            return -EIO;
        }
        fat_free_dir_iterator(iter);
    }

    // Reuse iterator dir buffer
    fat32_direntry_t* dir = iter->dir_entries;

    // Write dir entries to the buffer
    uint remaining_char_to_copy = lfn_len;
    char* p_filename = &file_entry->filename[lfn_len];
    fat32_set_short_name(file_entry);
    int res_numtail = fat32_set_numeric_tail(meta, iter, file_entry);
    if(res_numtail < 0) {
        return -ENOSPC;
    }
    fat32_direntry_short short_entry = file_entry->direntry;
    fat32_direntry_long long_entry = {.attr = FAT_ATTR_LFN, .csum = lfn_checksum(short_entry.nameext), .type=0x00, .reserved2=0x00};
    for(uint idx = 0; idx < dir_entry_needed; idx++) {
        if(idx == dir_entry_needed - 1) {
            // only short entry left to write
            dir[first_free_entry_idx + idx].short_entry = short_entry;
        } else {
            // Add LFN entry
            fat32_direntry_long e = long_entry;

            e.seq = lfn_entry_needed - idx;
            if(idx == 0) {
                // Mark first lfn entry
                e.seq += 0x40; 
            }

            uint char_to_copy;
            if(remaining_char_to_copy % FAT32_USC2_FILE_NAME_LEN_PER_LFN != 0) {
                char_to_copy = remaining_char_to_copy % FAT32_USC2_FILE_NAME_LEN_PER_LFN;
            } else {
                char_to_copy = FAT32_USC2_FILE_NAME_LEN_PER_LFN;
            }
            remaining_char_to_copy -= char_to_copy;
            p_filename -= char_to_copy;

            memset(e.name1, 0xFF, 5*2);
            memset(e.name2, 0xFF, 6*2);
            memset(e.name3, 0xFF, 2*2);
            assert(5+6+2 == FAT32_USC2_FILE_NAME_LEN_PER_LFN);
            for(uint i=0; i<char_to_copy; i++) {
                if(p_filename[i] == 0) {
                    break;
                }
                if(i<5) {
                    e.name1[i] = (uint16_t) p_filename[i];
                } else if(i<5+6) {
                    e.name2[i-5] = (uint16_t) p_filename[i];
                } else {
                    e.name3[i-(5+6)] = (uint16_t) p_filename[i];
                }
            }

            if(char_to_copy < FAT32_USC2_FILE_NAME_LEN_PER_LFN) {
                // Add NULL termination
               if(char_to_copy < 5) {
                   e.name1[char_to_copy] = 0;
               } else if(char_to_copy < 5+6) {
                   e.name2[char_to_copy-5] = 0;
               } else {
                   e.name3[char_to_copy-(5+6)] = 0;
               }
            }
            dir[first_free_entry_idx + idx].long_entry = e;
        }
    }

    // Write the dir to disk
    int64_t res = fat32_write_to_offset(meta, iter->first_cluster, first_free_entry_idx*sizeof(fat32_direntry_t), dir_entry_needed*sizeof(fat32_direntry_t), (uint8_t*) &dir[first_free_entry_idx]);
    if(res < 0) {
        return res;
    }


    return dir_entry_needed;

}

//Return: dir entries removed
static int fat32_rm_file_entry(fat32_meta* meta, fat_dir_iterator* iter, fat32_file_entry* file_entry)
{
    // Reuse iterator dir buffer, need to ensure the iter is initialized
    fat32_file_entry entry = {0};
    fat32_iterate_dir(meta, iter, &entry);
    fat32_direntry_t* dir = iter->dir_entries;

    // Write dir entries to the buffer
    for(uint i = 0; i < file_entry->dir_entry_count; i++) {
        // Set dir entry as deleted
        dir[file_entry->first_dir_entry_idx + i].short_entry.name[0] = 0xE5;
    }

    // Write the dir to disk
    int64_t res = fat32_write_to_offset(meta, iter->first_cluster, file_entry->first_dir_entry_idx*sizeof(fat32_direntry_t), file_entry->dir_entry_count*sizeof(fat32_direntry_t), (uint8_t*) &dir[file_entry->first_dir_entry_idx]);
    if(res < 0) {
        return res;
    }

    return file_entry->dir_entry_count;
}

// Create new dir entry, return parent dir cluster number if success
static int64_t fat32_create_new(fat32_meta* meta, const char *path, fat32_direntry_short* short_dir_entry)
{
    // TODO: Test for illegal path (characters / length etc.)
    fat32_file_entry file_entry = {0};
    fat_resolve_path_status status = fat32_resolve_path(meta, path, &file_entry);

    if(status == FAT_PATH_RESOLVE_ERROR) {
        return -EIO;
    }
    if(status == FAT_PATH_RESOLVE_INVALID_PATH) {
        return -ENOENT;
    }
    if(status == FAT_PATH_RESOLVE_FOUND || status == FAT_PATH_RESOLVE_ROOT_DIR) {
        return -EEXIST;
    }
    assert(status == FAT_PATH_RESOLVE_NOT_FOUND);
    // when resolve returns not found, the file entry is filled with last entry in the target dir
    // so we can use it to retrieve the dir cluster info 
    fat_dir_iterator iter = {.first_cluster = file_entry.dir_cluster};
    uint path_len = strlen(path);
    assert(path_len > 1);
    char* filename = (char*) &path[path_len-1];
    while(*filename == '/') {
        // skip trailing '/'
        *filename = 0;
        filename--;
    }
    if(filename < path) {
        return -EPERM;
    }
    // Get file name from path
    while(*filename!='/' && filename>path) {
        filename--;
    }
    filename++;

    strcpy(file_entry.filename, filename);
    file_entry.direntry = *short_dir_entry;
    uint16_t date, time;
    fat32_set_timestamp(&date, &time);
    file_entry.direntry.ctime_date = date;
    file_entry.direntry.ctime_time = time;
    file_entry.direntry.mtime_date = date;
    file_entry.direntry.mtime_time = time;

    int res = fat32_add_file_entry(meta, &iter, &file_entry);
    *short_dir_entry = file_entry.direntry;
    fat_free_dir_iterator(&iter);
    if(res < 0) {
        return res;
    }

    return iter.first_cluster;
}


static int fat32_update_file_entry(fat32_meta* meta, fat32_file_entry* file_entry)
{
    fat_dir_iterator iter = {.first_cluster = file_entry->dir_cluster};
    int dir_res = fat32_rm_file_entry(meta, &iter, file_entry);
    if(dir_res<0) {
        return dir_res;
    }
    dir_res = fat32_add_file_entry(meta, &iter, file_entry);
    if(dir_res<0) {
        return dir_res;
    }
    fat_free_dir_iterator(&iter);
    return 0;
}


static int fat32_readdir(struct fs_mount_point* mount_point, const char * path, uint offset, struct fs_dir_filler_info* info, fs_dir_filler filler)
{
    fat32_meta* meta = (fat32_meta*) mount_point->fs_meta;

    fat32_file_entry file_entry = {0};
    fat_resolve_path_status status = fat32_resolve_path(meta, path, &file_entry);

    fat_dir_iterator iter = {0};
    fat_iterate_dir_status iter_status; 

    if(status == FAT_PATH_RESOLVE_INVALID_PATH) {
        return -ENOTDIR;
    }
    if(status == FAT_PATH_RESOLVE_ERROR) {
        return -EIO;
    }
    if(status == FAT_PATH_RESOLVE_NOT_FOUND) {
        return -ENOENT;
    }
    if(status == FAT_PATH_RESOLVE_ROOT_DIR) {
        iter.first_cluster = meta->bootsector->root_cluster;
    }
    if(status == FAT_PATH_RESOLVE_FOUND) {
        if(!HAS_ATTR(file_entry.direntry.attr, FAT_ATTR_DIRECTORY)) {
            return -ENOTDIR;
        }
        iter.first_cluster = file_entry.direntry.cluster_lo + (file_entry.direntry.cluster_hi << 16);
    }

    while(1) {
        iter_status = fat32_iterate_dir(meta, &iter, &file_entry);
        if(iter_status == FAT_DIR_ITER_ERROR) {
            // Any error will discard all info we got
            fat_free_dir_iterator(&iter);
            return -EIO;
        }
        if(iter_status == FAT_DIR_ITER_DELETED) {
            continue;
        }
        if(iter_status == FAT_DIR_ITER_NO_MORE_ENTRY || iter_status == FAT_DIR_ITER_FREE_ENTRY) {
            fat_free_dir_iterator(&iter);
            return 0;
        }
        assert(iter_status == FAT_DIR_ITER_VALID_ENTRY || iter_status == FAT_DIR_ITER_DOT_ENTRY);
        if(HAS_ATTR(file_entry.direntry.attr, FAT_ATTR_VOLUME_ID)) {
            // Skip Volume label entry when listing directory
            continue;
        }

        if(offset == 0) {
            if(filler(info, (char*) file_entry.filename, NULL) != 0) {
                // if filler's internal buffer is full, return
                return 0;
            }
        } else {
            offset--;
        }
        
    }
}


static int fat32_getattr(struct fs_mount_point* mount_point, const char * path, struct fs_stat * st, struct fs_file_info *fi)
{
	(void) fi;

    fat32_meta* meta = (fat32_meta*) mount_point->fs_meta;

	memset(st, 0, sizeof(*st));
    st->mount_point_id = mount_point->id;

    fat32_file_entry file_entry = {0};
    fat_resolve_path_status status = fat32_resolve_path(meta, path, &file_entry);

    uint bytes_per_cluster = meta->bootsector->bytes_per_sector*meta->bootsector->sectors_per_cluster;

    if(status == FAT_PATH_RESOLVE_ROOT_DIR) {
        // For root dir
        st->mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
        st->nlink = 2;
        uint cluster_number = meta->bootsector->root_cluster;
        st->inum = cluster_number;
        st->size = count_clusters(meta, cluster_number)*bytes_per_cluster;
        st->blocks = st->size/512;
        return 0;
    }
    if(status == FAT_PATH_RESOLVE_INVALID_PATH) {
        return -ENOTDIR;
    }
    if(status == FAT_PATH_RESOLVE_ERROR) {
        return -EIO;
    }
    if(status == FAT_PATH_RESOLVE_NOT_FOUND) {
        return -ENOENT;
    }

    if(status == FAT_PATH_RESOLVE_FOUND) {
        if(HAS_ATTR(file_entry.direntry.attr, FAT_ATTR_READ_ONLY)) {
            st->mode = S_IRUSR | S_IRGRP | S_IROTH;
        } else {
            st->mode = S_IRWXU | S_IRWXG | S_IRWXO;
        }
        uint cluster_number = file_entry.direntry.cluster_lo + (file_entry.direntry.cluster_hi << 16);
        if (HAS_ATTR(file_entry.direntry.attr, FAT_ATTR_DIRECTORY)) {
            st->mode |= S_IFDIR;
            st->nlink = 2;
            st->inum = cluster_number;
            st->size = count_clusters(meta, cluster_number)*bytes_per_cluster;
            st->blocks = st->size/512;
        } else {
            st->mode |= S_IFREG;
            st->nlink = 1;
            st->size = file_entry.direntry.size;
            st->inum = cluster_number;
            st->blocks = count_clusters(meta, cluster_number)*bytes_per_cluster/512;
        }
    }
	st->mtime = convert_datetime(file_entry.direntry.mtime_date, file_entry.direntry.mtime_time);
	st->ctime = convert_datetime(file_entry.direntry.ctime_date, file_entry.direntry.ctime_time);

    return 0;
}

static int fat32_read(struct fs_mount_point* mount_point, const char * path, char *buf, uint size, uint offset, struct fs_file_info *fi)
{

    fat32_meta* meta = (fat32_meta*) mount_point->fs_meta;

    fat32_file_entry file_entry = {0};
    if(fi != NULL) {
        file_entry = meta->file_table[fi->fh];
        assert(file_entry.dir_entry_count > 0);
    } else {
        fat_resolve_path_status status = fat32_resolve_path(meta, path, &file_entry);

        if(status == FAT_PATH_RESOLVE_ROOT_DIR) {
            return -EISDIR;
        }
        if(status == FAT_PATH_RESOLVE_INVALID_PATH) {
            return -ENOENT;
        }
        if(status == FAT_PATH_RESOLVE_ERROR) {
            return -EIO;
        }
        if(status == FAT_PATH_RESOLVE_NOT_FOUND) {
            return -ENOENT;
        }

        assert(status == FAT_PATH_RESOLVE_FOUND);
    }

    if(HAS_ATTR(file_entry.direntry.attr, FAT_ATTR_DIRECTORY)) {
        return -EISDIR;
    }

    if(offset >= file_entry.direntry.size) {
        return 0;
    }

    if(offset + size > file_entry.direntry.size) {
        size = file_entry.direntry.size - offset;
    }

    fat_cluster cluster;
    cluster.next = file_entry.direntry.cluster_lo + (file_entry.direntry.cluster_hi << 16);
    uint bytes_per_cluster = meta->bootsector->sectors_per_cluster*meta->bootsector->bytes_per_sector;
    uint8_t* cluster_buffer = malloc(bytes_per_cluster);
    uint size_in_this_cluster;
    int64_t total_bytes_read = 0;
    while(1) {
        fat_cluster_status cluster_status = fat32_get_cluster_info(meta, cluster.next, &cluster);

        if(cluster_status == FAT_CLUSTER_BAD || cluster_status == FAT_CLUSTER_FREE || cluster_status == FAT_CLUSTER_RESERVED) {
            free(cluster_buffer);
            return -EIO;
        }
        assert(cluster_status == FAT_CLUSTER_USED || cluster_status == FAT_CLUSTER_EOC);

        if(offset < bytes_per_cluster) {
            int64_t read_res = fat32_read_clusters(meta, cluster.curr, 1, cluster_buffer);
            if(read_res < 0) {
                free(cluster_buffer);
                return read_res;
            }
            if(size <= bytes_per_cluster - offset) {
                size_in_this_cluster = size;
            } else {
                size_in_this_cluster = bytes_per_cluster - offset;
            }
            memmove(buf, cluster_buffer + offset, size_in_this_cluster);
            buf += size_in_this_cluster;
            offset = 0;
            size -= size_in_this_cluster;
            total_bytes_read += size_in_this_cluster;
        } else {
            offset -= bytes_per_cluster;
        }

        if(size == 0) {
            free(cluster_buffer);
            return total_bytes_read;
        }

        if(cluster_status == FAT_CLUSTER_EOC) {
            return -EIO;
        }
    }
}

static int fat32_mknod(struct fs_mount_point* mount_point, const char * path, uint mode)
{
    (void) mode;

    fat32_meta* meta = (fat32_meta*) mount_point->fs_meta;

    fat32_direntry_short short_dir_entry = {0};
    int64_t parent_cluster = fat32_create_new(meta, path, &short_dir_entry);
    if(parent_cluster < 0) {
        return -parent_cluster;
    }
    return 0;
}

static int fat32_mkdir(struct fs_mount_point* mount_point, const char * path, uint mode)
{
    (void) mode;

    fat32_meta* meta = (fat32_meta*) mount_point->fs_meta;
    
    fat32_direntry_short short_dir_entry = {.attr = FAT_ATTR_DIRECTORY};
    uint first_new_cluster = fat32_allocate_cluster(meta, 0, 1);
    if(first_new_cluster == 0) {
        return -EIO;
    }
    short_dir_entry.cluster_lo = first_new_cluster & 0x0000FFFF;
    short_dir_entry.cluster_hi = first_new_cluster >> 16;
    int64_t parent_cluster = fat32_create_new(meta, path, &short_dir_entry);
    if(parent_cluster < 0) {
        return parent_cluster;
    }
    
    // Add dot entries
    uint cluster_byte_size = meta->bootsector->bytes_per_sector*meta->bootsector->sectors_per_cluster;
    fat32_direntry_t* dir_buff = malloc(cluster_byte_size);
    memset(dir_buff, 0, cluster_byte_size);
    // Add .
    dir_buff[0].short_entry = short_dir_entry;
    memset(dir_buff[0].short_entry.nameext, ' ', FAT_SHORT_NAME_LEN+FAT_SHORT_EXT_LEN);
    dir_buff[0].short_entry.name[0] = '.';
    // Add ..
    dir_buff[1] = dir_buff[0];
    dir_buff[1].short_entry.name[1] = '.';
    dir_buff[1].short_entry.cluster_hi = parent_cluster >> 16;
    dir_buff[1].short_entry.cluster_lo = parent_cluster & 0x0000FFFF;
    int64_t res = fat32_write_clusters(meta, first_new_cluster, 1, (uint8_t*) dir_buff);
    free(dir_buff);
    if(res < 0) {
        return -EIO;
    }
    return 0;
}

static int fat32_is_cluster_opened(fat32_meta* meta, uint cluster)
{
    if(cluster <= 1) {
        return 0;
    }
    for(int i=0;i<FAT32_N_OPEN_FILE;i++) {
        uint opened_cluster =  meta->file_table[i].direntry.cluster_lo + (meta->file_table[i].direntry.cluster_hi << 16);
        if(cluster == opened_cluster) {
            return 1;
        }
    }
    return 0;
}

//return: 0 - not empty, 1 - empty, <0 - error
static int fat32_is_dir_empty(fat32_meta* meta, fat_dir_iterator* iter)
{
    fat32_file_entry file_in_dir = {0};

    while(1) {
        fat_iterate_dir_status iter_status = fat32_iterate_dir(meta,iter,&file_in_dir);
        if(iter_status == FAT_DIR_ITER_ERROR) {
            // Any error will discard all info we got
            fat_free_dir_iterator(iter);
            return -EIO;
        }
        if(iter_status == FAT_DIR_ITER_DELETED || iter_status == FAT_DIR_ITER_DOT_ENTRY) {
            continue;
        }
        if(iter_status == FAT_DIR_ITER_NO_MORE_ENTRY || iter_status == FAT_DIR_ITER_FREE_ENTRY) {
            // Dir is empty
            fat_free_dir_iterator(iter);
            return 1;
        }
        assert(iter_status == FAT_DIR_ITER_VALID_ENTRY);
        if(HAS_ATTR(file_in_dir.direntry.attr, FAT_ATTR_VOLUME_ID)) {
            // Skip Volume label entry when listing directory
            continue;
        }
        // Dir is not empty
        fat_free_dir_iterator(iter);
        return 0;
    }
}

static int fat32_rm(fat32_meta* meta, const char * path, enum fat32_rm_type rm_type)
{
    fat32_file_entry file_entry = {0};
    fat_resolve_path_status status = fat32_resolve_path(meta, path, &file_entry);

    if(status == FAT_PATH_RESOLVE_ERROR) {
        return -EIO;
    }
    if(status == FAT_PATH_RESOLVE_NOT_FOUND || status == FAT_PATH_RESOLVE_INVALID_PATH) {
        return -ENOENT;
    }
    if(status == FAT_PATH_RESOLVE_ROOT_DIR) {
        // not allow to unlink/delete root dir
        return -EPERM;
    }
    assert(status == FAT_PATH_RESOLVE_FOUND);

    uint cluster = file_entry.direntry.cluster_lo + (file_entry.direntry.cluster_hi << 16);
    if(fat32_is_cluster_opened(meta, cluster)) {
        // shall not delete opened file
        return -EPERM;
    }

    if(HAS_ATTR(file_entry.direntry.attr, FAT_ATTR_DIRECTORY)) {
        if(rm_type == FAT32_RM_FILE) {
            return -ENOTDIR;
        }
        fat_dir_iterator iter = {.first_cluster = cluster};
        int is_empty = fat32_is_dir_empty(meta, &iter);
        if(is_empty < 0) {
            return is_empty;
        }
        if(!is_empty) {
            // Do not allow deleting non-empty dir
            return -EPERM;
        }
    } else {
        if(rm_type == FAT32_RM_DIR) {
            return -EISDIR;
        }
    }

    fat_dir_iterator iter = {.first_cluster = file_entry.dir_cluster};
    int res = fat32_rm_file_entry(meta, &iter, &file_entry);
    fat_free_dir_iterator(&iter);
    if(res < 0) {
        return res;
    }

    // Free data clusters
    res = fat32_free_cluster(meta, 0, cluster, 0); // free the whole cluster chain
    if(res < 0) {
        return res;
    }

    return 0;
}

static int fat32_unlink(struct fs_mount_point* mount_point, const char * path)
{
    fat32_meta* meta = (fat32_meta*) mount_point->fs_meta;
    int res = fat32_rm(meta, path, FAT32_RM_FILE);
    return res;
}

static int fat32_rmdir(struct fs_mount_point* mount_point, const char * path)
{
    fat32_meta* meta = (fat32_meta*) mount_point->fs_meta;
    int res = fat32_rm(meta, path, FAT32_RM_DIR);
    return res;
}

static int fat32_write(struct fs_mount_point* mount_point, const char * path, const char *buf, uint size, uint offset, struct fs_file_info * fi)
{
    fat32_meta* meta = (fat32_meta*) mount_point->fs_meta;

    fat32_file_entry file_entry = {0};
    if(fi != NULL) {
        file_entry = meta->file_table[fi->fh];
        assert(file_entry.dir_entry_count > 0);
    } else {
        fat_resolve_path_status status = fat32_resolve_path(meta, path, &file_entry);

        if(status == FAT_PATH_RESOLVE_ROOT_DIR) {
            return -EISDIR;
        }
        if(status == FAT_PATH_RESOLVE_INVALID_PATH) {
            return -ENOENT;
        }
        if(status == FAT_PATH_RESOLVE_ERROR) {
            return -EIO;
        }
        if(status == FAT_PATH_RESOLVE_NOT_FOUND) {
            return -ENOENT;
        }

        assert(status == FAT_PATH_RESOLVE_FOUND);
    }

    if(HAS_ATTR(file_entry.direntry.attr, FAT_ATTR_DIRECTORY)) {
        return -EISDIR;
    }

    uint first_cluster = file_entry.direntry.cluster_lo + (file_entry.direntry.cluster_hi << 16);

    uint cluster_count = first_cluster == 0 ? 0 : count_clusters(meta, first_cluster);
    uint bytes_per_cluster = meta->bootsector->sectors_per_cluster*meta->bootsector->bytes_per_sector;
    uint allocated_size = bytes_per_cluster * cluster_count;
    if(offset + size > allocated_size) {
        uint clusters_to_allocate = ((offset + size) - allocated_size - 1) / bytes_per_cluster + 1;
        uint first_allocated_cluster = fat32_allocate_cluster(meta, fat32_index_cluster_chain(meta, first_cluster, -1), clusters_to_allocate);
        if(first_allocated_cluster == 0) {
            return -EIO;
        }
        if(first_cluster == 0) {
            first_cluster = first_allocated_cluster;
            file_entry.direntry.cluster_lo = first_allocated_cluster & 0x0000FFFF;
            file_entry.direntry.cluster_hi = first_allocated_cluster >> 16;
        }
    }

    if(offset + size > file_entry.direntry.size) {
        file_entry.direntry.size = offset + size;
    }

    uint16_t date, time;
    fat32_set_timestamp(&date, &time);
    file_entry.direntry.mtime_time = time;
    file_entry.direntry.mtime_date = date;

    int dir_res = fat32_update_file_entry(meta, &file_entry);
    if(dir_res < 0) {
        return dir_res;
    }

    if(size == 0) {
        return 0;
    }

    int write_res = fat32_write_to_offset(meta, first_cluster, offset, size, (uint8_t*) buf);
    if(write_res < 0) {
        return write_res;
    }

    return size;
}

static int fat32_truncate(struct fs_mount_point* mount_point, const char * path, uint size, struct fs_file_info *fi)
{
    fat32_meta* meta = (fat32_meta*) mount_point->fs_meta;
    
    fat32_file_entry file_entry = {0};
    if(fi != NULL) {
        file_entry = meta->file_table[fi->fh];
        assert(file_entry.dir_entry_count > 0);
    } else {
        fat_resolve_path_status status = fat32_resolve_path(meta, path, &file_entry);

        if(status == FAT_PATH_RESOLVE_ROOT_DIR) {
            return -EISDIR;
        }
        if(status == FAT_PATH_RESOLVE_INVALID_PATH) {
            return -ENOENT;
        }
        if(status == FAT_PATH_RESOLVE_ERROR) {
            return -EIO;
        }
        if(status == FAT_PATH_RESOLVE_NOT_FOUND) {
            return -ENOENT;
        }

        assert(status == FAT_PATH_RESOLVE_FOUND);
    }

    if(HAS_ATTR(file_entry.direntry.attr, FAT_ATTR_DIRECTORY)) {
        return -EISDIR;
    }

    if(size == file_entry.direntry.size) {
        // Same size, do nothing
        return 0;
    }

    uint first_cluster = file_entry.direntry.cluster_lo + (file_entry.direntry.cluster_hi << 16);
    uint orig_size = file_entry.direntry.size;

    uint cluster_count = first_cluster == 0 ? 0 : count_clusters(meta, first_cluster);
    uint bytes_per_cluster = meta->bootsector->sectors_per_cluster*meta->bootsector->bytes_per_sector;
    uint allocated_size = bytes_per_cluster * cluster_count;
    if(size > allocated_size) {
        uint clusters_to_allocate = (size - allocated_size - 1) / bytes_per_cluster + 1;
        uint first_allocated_cluster = fat32_allocate_cluster(meta, fat32_index_cluster_chain(meta, first_cluster, -1), clusters_to_allocate);
        if(first_allocated_cluster == 0) {
            return -EIO;
        }
        if(first_cluster == 0) {
            first_cluster = first_allocated_cluster;
            file_entry.direntry.cluster_lo = first_allocated_cluster & 0x0000FFFF;
            file_entry.direntry.cluster_hi = first_allocated_cluster >> 16;
        }
    } else if(allocated_size - size >= bytes_per_cluster){
        int clusters_to_free = (allocated_size - size) / bytes_per_cluster;
        uint first_cluster_to_free = fat32_index_cluster_chain(meta, first_cluster, -clusters_to_free);
        uint last_remaining_cluster;
        if(first_cluster_to_free == first_cluster) {
            last_remaining_cluster = 0;
            file_entry.direntry.cluster_lo = 0;
            file_entry.direntry.cluster_hi = 0;
        } else {
            last_remaining_cluster = fat32_index_cluster_chain(meta, first_cluster, -clusters_to_free - 1);
        }
        int free_res = fat32_free_cluster(meta, last_remaining_cluster, first_cluster_to_free, 0);
        if(free_res < 0) {
            return free_res;
        }
    }
    file_entry.direntry.size = size;
    uint16_t date, time;
    fat32_set_timestamp(&date, &time);
    file_entry.direntry.mtime_time = time;
    file_entry.direntry.mtime_date = date;

    int dir_res = fat32_update_file_entry(meta, &file_entry);
    if(dir_res < 0) {
        return dir_res;
    }

    if(size == 0) {
        return 0;
    }

    if(size > orig_size) {
        // Zero out the allocated space
        uint8_t* zeros = malloc(size - orig_size);
        memset(zeros, 0, size - orig_size);
        int write_res = fat32_write_to_offset(meta, first_cluster, orig_size, size - orig_size, zeros);
        free(zeros);
        if(write_res < 0) {
            return write_res;
        }
    }

    return 0;
}

static int fat32_rename(struct fs_mount_point* mount_point, const char * from, const char * to, uint flags)
{
    fat32_meta* meta = (fat32_meta*) mount_point->fs_meta;

    (void) flags;
    
    if(strcmp(from, to) == 0) {
        return 0;
    }

    fat32_file_entry from_file_entry = {0};
    fat_resolve_path_status from_status = fat32_resolve_path(meta, from, &from_file_entry);

    if(from_status == FAT_PATH_RESOLVE_ROOT_DIR) {
        return -EPERM;
    }
    if(from_status == FAT_PATH_RESOLVE_INVALID_PATH) {
        return -ENOENT;
    }
    if(from_status == FAT_PATH_RESOLVE_ERROR) {
        return -EIO;
    }
    if(from_status == FAT_PATH_RESOLVE_NOT_FOUND) {
        return -ENOENT;
    }

    assert(from_status == FAT_PATH_RESOLVE_FOUND);

    uint from_file_content_cluster_number = from_file_entry.direntry.cluster_lo + (from_file_entry.direntry.cluster_hi << 16);
    // Shall not rename file opened
    if(fat32_is_cluster_opened(meta, from_file_content_cluster_number)) {
        return -EBUSY;
    }

    // Remove anything at the "to" path, if exist
    int rm_res = fat32_rm(meta, to, FAT32_RM_ANY);
    if(rm_res < 0 && rm_res != -ENOENT) {
        return rm_res;
    }

    int64_t parent_cluster = fat32_create_new(meta, to, &from_file_entry.direntry);
    if(parent_cluster < 0) {
        return parent_cluster;
    }

    // Remove old dir entry
    fat_dir_iterator iter = {.first_cluster = from_file_entry.dir_cluster};
    int dir_res = fat32_rm_file_entry(meta, &iter, &from_file_entry);
    fat_free_dir_iterator(&iter);
    if(dir_res<0) {
        return dir_res;
    }

	return 0;
}

static int fat32_open(struct fs_mount_point* mount_point, const char * path, struct fs_file_info *fi)
{
    fat32_meta* meta = (fat32_meta*) mount_point->fs_meta;

    fat32_file_entry file_entry = {0};
    fat_resolve_path_status status = fat32_resolve_path(meta, path, &file_entry);

    if(status == FAT_PATH_RESOLVE_ROOT_DIR) {
        return -EISDIR;
    }
    if(status == FAT_PATH_RESOLVE_INVALID_PATH) {
        return -ENOENT;
    }
    if(status == FAT_PATH_RESOLVE_ERROR) {
        return -EIO;
    }
    if(status == FAT_PATH_RESOLVE_FOUND) {
        if(HAS_ATTR(file_entry.direntry.attr, FAT_ATTR_DIRECTORY)) {
            return -EISDIR;
        }
        if(HAS_ATTR(fi->flags, O_EXCL)) {
            // O_EXCL Ensure that this call creates the file
            return -EEXIST;
        }
    } else {
        assert(status == FAT_PATH_RESOLVE_NOT_FOUND);
        if(HAS_ATTR(fi->flags, O_CREAT)) {
            fat32_direntry_short short_dir_entry = {0};
            int64_t parent_cluster = fat32_create_new(meta, path, &short_dir_entry);
            if(parent_cluster < 0) {
                return parent_cluster;
            }
        }
        if(HAS_ATTR(fi->flags, O_TRUNC)) {
            int res = fat32_truncate(mount_point, path, 0, fi);
            if(res < 0) {
                return res;
            }
        }
        status = fat32_resolve_path(meta, path, &file_entry);
        assert(status == FAT_PATH_RESOLVE_FOUND);
    }
    
    // Save the resolved file entry to the file table
    //   in order to reuse the path resolution result
    assert(file_entry.dir_entry_count > 0);
    uint i;
    for(i=0; i<FAT32_N_OPEN_FILE; i++) {
        if(meta->file_table[i].dir_entry_count == 0) {
            meta->file_table[i] = file_entry;
            break;
        }
        if(i == FAT32_N_OPEN_FILE-1) {
            return -ENFILE;
        } 
    }
    // Set FUSE file handle
    fi->fh = i;

	return 0;
}

static int fat32_release(struct fs_mount_point* mount_point, const char * path, struct fs_file_info *fi)
{
	(void) path;
    (void) fi;
    
    fat32_meta* meta = (fat32_meta*) mount_point->fs_meta;

    assert(meta->file_table[fi->fh].dir_entry_count > 0);
    // Clear file table entry
    memset(&meta->file_table[fi->fh], 0, sizeof(fat32_file_entry));

	return 0;
}

static int fat32_mount(fs_mount_point* mount_point, void* option)
{
    fat_mount_option* opt = (fat_mount_option*) option;
    
    fat32_meta* meta = malloc(sizeof(fat32_meta));
    memset(meta, 0, sizeof(*meta));
    mount_point->fs_meta = meta;
    mount_point->operations = (struct file_system_operations) {
        .release = fat32_release,
        .open = fat32_open,
        .mknod = fat32_mknod,
        .mkdir = fat32_mkdir,
        .read = fat32_read,
        .write = fat32_write,
        .readdir = fat32_readdir,
        .getattr = fat32_getattr,
        .rename = fat32_rename,
        .rmdir = fat32_rmdir,
        .unlink = fat32_unlink,
        .truncate = fat32_truncate
    };

    meta->storage = opt->storage;
    // Read header
    int res = fat32_get_meta(meta);
    if(res != 0) {
        return res;
    }

    return 0;
}

static int fat32_unmount(fs_mount_point* mount_point)
{
    free(mount_point->fs_meta);
    return 0;
}

int fat32_init(struct file_system* fs)
{
    fs->mount = fat32_mount;
    fs->unmount = fat32_unmount;
    fs->fs_global_meta = NULL;
    fs->status = FS_STATUS_READY;
    return 0;
}

