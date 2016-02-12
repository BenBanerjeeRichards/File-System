#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
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
	superblock->inode_table_start_addr = 1 + (superblock->inode_bitmap_size / superblock->block_size);
	superblock->data_block_bitmap_addr = superblock->inode_table_start_addr + (superblock->inode_table_size / superblock->block_size);

	// blocks_remaining is all of the blocks which have not been reserved up to this point
	int blocks_remaining = superblock->num_blocks - superblock->data_block_bitmap_addr;
	superblock->num_data_blocks = ceil((8.0/9.0) * blocks_remaining);
	superblock->data_block_bitmap_size = (blocks_remaining - superblock->num_data_blocks) * superblock->block_size;

	superblock->data_blocks_start_addr = superblock->data_block_bitmap_addr + (superblock->data_block_bitmap_size / superblock->block_size);

	superblock->data_bitmap_circular_loc = 0;
	superblock->flags= 0;

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

	uint8_t byte_val = mem_read(*bitmap, byte_addr, &ret);
	if (ret != SUCCESS) return ret;

	if (value) {
		byte_val |= 1 << (7 - bit);
	} else {
		byte_val &= ~(1 << (7 - bit));
	}

	return mem_write(bitmap, byte_addr, byte_val);
}

int fs_read_bitmap_bit(Bitmap bitmap, int bit_address, int* error)
{
	int byte_addr = bit_address / 8;
	int bit = bit_address - (byte_addr * 8);
	int ret = 0;

	uint8_t byte = mem_read(bitmap, byte_addr, &ret);
	if (ret != SUCCESS) return ret;

	return (byte >> (7-bit)) & 1;
}

int fs_add_directory_entry(Directory* directory, DirectoryEntry entry) {
	if (entry.name.size > 0xFF) return ERR_INODE_NAME_TOO_LARGE;
	const int entry_size = entry.name.size + 5;
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

	ret = mem_write(directory, current_location, entry.name.size);
	if (ret != SUCCESS) return ret;
	current_location += INCREMENT_8;

	ret = mem_write_section(directory, current_location, &entry.name);
	if (ret != SUCCESS) return ret;

	return SUCCESS;
}

int fs_directory_get_inode_number(Directory directory, HeapData name, uint32_t* inode_number) {
	if(!directory.valid) return ERR_INVALID_MEMORY_ACCESS;
	if (!name.valid) return ERR_INVALID_MEMORY_ACCESS;

	// Always referes to the *start* of each directory entry
	int current_location = 0;

	while (current_location < directory.size) {
		int name_start = current_location + 5;
		int error = 0;
		if (name_start + name.size > directory.size) return ERR_INODE_NOT_FOUND;

		uint32_t inode_num = util_read_uint32(directory, current_location, &error);
		if (error != SUCCESS) return error;

		uint8_t name_size = mem_read(directory, current_location + 4, &error);
		if (error != SUCCESS) return error;

		if (name_size != name.size) {
			current_location += 5 + name_size;
			continue;
		}

		if (memcmp(&directory.data[current_location + 5], name.data, name.size) == 0) {
			*inode_number = inode_num;
			return SUCCESS;
		}

		current_location += 5 + name_size;
	}

	return ERR_INODE_NOT_FOUND;
}

int fs_find_continuous_bitmap_run(Bitmap bitmap, int length, int start_byte, int* run_start_bit) {
	if (!bitmap.valid) return ERR_INVALID_BITMAP;	
	int current_bit_count = 0;
	
	for (int i = 0; i < bitmap.size; i++)
	{
		int byte_index = 0;
		if(i + start_byte + 1 > bitmap.size) {
			byte_index = (i + start_byte) - bitmap.size;
		} else {
			byte_index = i + start_byte;
		}
		
		int error = 0;
		for (int j = 0; j < 8; j++) {
			int bit = fs_read_bitmap_bit(bitmap, byte_index * 8 + j, &error);

			if (error != SUCCESS) return error;

			if (bit == 0) {
				current_bit_count += 1;
			} else{
				current_bit_count = 0;
			}
			
			if (current_bit_count == length) {
				*run_start_bit = byte_index * 8 + j - current_bit_count;
				return SUCCESS;
			}

		}
	}
	
	return SUCCESS;
}
