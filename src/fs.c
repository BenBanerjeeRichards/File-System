#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "disk.h"
#include "constants.h"
#include "memory.h"
#include "fs.h"
#include "util.h"

int fs_create_superblock(Superblock* superblock, uint64_t partition_size){
	// Ensure that the superblock is initialised before passing to this func.
	superblock->magic_1 = SUPERBLOCK_MAGIC_1;
	superblock->magic_2 = SUPERBLOCK_MAGIC_2;
	superblock->version = CURRENT_FS_VERSION;

	superblock->block_size = BLOCK_SIZE;
	superblock->num_blocks = (uint64_t)floorl(partition_size / BLOCK_SIZE);
	superblock->num_used_blocks = 0;

	superblock->inode_size = INODE_SIZE;
	superblock->num_inodes = powl(2, ceil(log2(0.01 * partition_size))) / superblock->inode_size;
	superblock->num_used_inodes = 0;

	uint32_t inode_bitmap_size = (uint32_t) ceil(superblock->num_inodes / 8);

	// Fit the bitmaps into full blocks
	superblock->inode_bitmap_size = round_up_nearest_multiple(superblock->num_inodes / 8, superblock->block_size);

	superblock->inode_table_size = round_up_nearest_multiple(superblock->num_inodes * superblock->inode_size, superblock->block_size);
  	superblock->inode_start_addr = 1 + (superblock->inode_bitmap_size / superblock->block_size);
  	superblock->data_block_bitmap_addr = superblock->inode_start_addr + (superblock->inode_table_size / superblock->block_size);

  	// blocks_remaining is all of the blocks which have not been reserved up to this point
  	int blocks_remaining = superblock->num_blocks - superblock->data_block_bitmap_addr;
  	superblock->num_data_blocks = ceil((8.0/9.0) * blocks_remaining);
  	superblock->data_block_bitmap_size = (blocks_remaining - superblock->num_data_blocks) * superblock->block_size;

  	superblock->data_blocks_start_addr = superblock->data_block_bitmap_addr + (superblock->data_block_bitmap_size / superblock->block_size);

	return SUCCESS;
}

int fs_write_block(HeapData* disk, HeapData block, int address) { 
	int location = BLOCK_SIZE * address;
	return mem_write_section(disk, location, &block);
}

int fs_write_bitmap_bit(Bitmap* bitmap, int bit_address, int value){
	// This works due to integer division
	int byte_addr = bit_address / 8;
	int bit = bit_address - (byte_addr * 8);
	int ret = 0;

	uint8_t byte_val = mem_read(bitmap, byte_addr, &ret);
	if (ret != SUCCESS) return ret;

	if (value) {
		byte_val |= 1 << (7 - bit);
	} else {
		byte_val &= ~(1 << (7 - bit));
	}

	return mem_write(bitmap, byte_addr, byte_val);
}

int fs_read_bitmap_bit(Bitmap* bitmap, int bit_address, int* error)
{
	int byte_addr = bit_address / 8;
	int bit = bit_address - (byte_addr * 8);
	int ret = 0;

	uint8_t byte = mem_read(bitmap, byte_addr, &ret);
	if (ret != SUCCESS) return ret;

	return (byte >> (7-bit)) & 1;
}

int fs_add_directory_entry(Directory* directory, DirectoryEntry entry) {
	const int entry_size = entry.filename_length + 5;
	int ret = 0;

	if (!directory->valid) {
		ret = mem_alloc(directory, entry_size);
	} else {
		ret = mem_realloc(directory, entry_size + directory->size);
	}
	if (ret != SUCCESS) return ret;
	
	const int INCREMENT_8 = 1;
	const int INCREMENT_16 = 2;
	const int INCREMENT_32 = 4;

	int current_location = directory->size - entry_size;

	ret = util_write_uint32(directory, current_location, entry.inode_number);
	if (ret != SUCCESS) return ret;
	current_location += INCREMENT_32;

	ret = mem_write(directory, current_location, entry.filename_length);
	if (ret != SUCCESS) return ret;
	current_location += INCREMENT_8;

	ret = mem_write_section(directory, current_location, &entry.filename);
	if (ret != SUCCESS) return ret;

	return SUCCESS;
}