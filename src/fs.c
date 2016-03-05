#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "disk.h"
#include "constants.h"
#include "memory.h"
#include "fs.h"
#include "util.h"
#include "stream.h"
#include "../../core/src/llist.h"

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

	ret = mem_write_section(directory, current_location, entry.name);
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

int fs_allocate_blocks(Disk* disk, int num_blocks, LList** addresses) {
	Superblock* sb = &disk->superblock;	
	double ft_ratio = (double)sb->num_used_blocks / (double)sb->num_blocks;
	*addresses = llist_new();
	(*addresses)->free_element = &free_element_standard;

	int ret = 0;
	if (ft_ratio < ALLOC_FULL_FT_MAX) {
		// Try and find the block run in a single try
		int start_bit = 0;
		ret = bitmap_find_continuous_block_run(disk->data_bitmap, 
				num_blocks, sb->data_bitmap_circular_loc, &start_bit);

		if (ret != SUCCESS && ret != ERR_NO_BITMAP_RUN_FOUND) return ret;
		if (ret == SUCCESS) {
			BlockSequence* seq = malloc(sizeof(BlockSequence));
			seq->length = num_blocks;
			seq->start_addr = start_bit;
			llist_insert(*addresses, seq);
			sb->data_bitmap_circular_loc = start_bit / 8;
		}

	}

	if (ret == ERR_NO_BITMAP_RUN_FOUND || ft_ratio >= ALLOC_FULL_FT_MAX) {
		// Grows as spaces are found
		int allocated_bytes = 0;
		// Where to start searching from, updated and wrapped around disk
		int current_byte = sb->data_bitmap_circular_loc;

		while (allocated_bytes < num_blocks) {
			int run_start_bit = 0;
			int length = 0;
			// Look for size of 1 then find total size
			// Doesn't work well for small allocations FIXME
			int r = bitmap_find_continuous_block_run(disk->data_bitmap, 1, current_byte, 
				&run_start_bit);

			if (r != SUCCESS) return r;
			
			r = bitmap_find_continuous_run_length(disk->data_bitmap, run_start_bit, &length);
			current_byte = (run_start_bit / 8) + 2 + (length / 8);
			allocated_bytes += length / 8;

			// Add allocation to LL
			BlockSequence* seq = malloc(sizeof(BlockSequence));
			seq->length = length;
			seq->start_addr = run_start_bit;
			llist_insert(*addresses, seq);

			sb->data_bitmap_circular_loc = run_start_bit / 8;
		}
	}

	return SUCCESS;
}

int fs_write_data_to_disk(Disk* disk, HeapData data, LList addresses) {
	if (!disk->data.valid) return ERR_INVALID_MEMORY_ACCESS;
	if (!data.valid) return ERR_INVALID_MEMORY_ACCESS;

	int bytes_written = 0;
	LListNode* current = addresses.head;
	while (bytes_written < data.size && current != NULL) {
		if (addresses.head == NULL) return ERR_TOO_FEW_ADDRESSES_PROVIDED;
		BlockSequence* seq = current->element;

		memcpy(&disk->data.data[seq->start_addr], &data.data[bytes_written], seq->length);
		bytes_written += seq->length;

		current = current->next;
	}


	return SUCCESS;
}
