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
	superblock->inode_bitmap_size_bytes = superblock->num_inodes / 8;
	superblock->inode_bitmap_size = round_up_nearest_multiple(superblock->inode_bitmap_size_bytes, superblock->block_size);

	superblock->inode_table_size = round_up_nearest_multiple(superblock->num_inodes * superblock->inode_size, superblock->block_size);
	superblock->inode_table_start_addr = 1 + (superblock->inode_bitmap_size / superblock->block_size);
	superblock->data_block_bitmap_addr = superblock->inode_table_start_addr + (superblock->inode_table_size / superblock->block_size);

	// blocks_remaining is all of the blocks which have not been reserved up to this point
	int blocks_remaining = superblock->num_blocks - superblock->data_block_bitmap_addr;
	superblock->num_data_blocks = ceil((8.0/9.0) * blocks_remaining);
	//superblock->data_block_bitmap_size = (blocks_remaining - superblock->num_data_blocks) * superblock->block_size;
	superblock->data_block_bitmap_size_bytes = superblock->num_data_blocks / 8;
	superblock->data_block_bitmap_size = round_up_nearest_multiple(superblock->data_block_bitmap_size_bytes, superblock->block_size);
	superblock->data_blocks_start_addr = superblock->data_block_bitmap_addr + (superblock->data_block_bitmap_size / superblock->block_size);

	superblock->data_bitmap_circular_loc = 0;
	superblock->flags= 0;

	return SUCCESS;
}

// Allocation policy for heavily fragmented disks
int _fs_allocate_fragmented(Disk* disk, int num_blocks, LList** addresses) {
	// Grows as spaces are found
	int allocated_bytes = 0;
	// Where to start searching from, updated and wrapped around disk
	int current_byte = disk->superblock.data_bitmap_circular_loc;

	while (allocated_bytes < num_blocks * BLOCK_SIZE) {
		int run_start_bit = 0;
		int length = 0;

		// Look for size of 1 then find total size
		int r = bitmap_find_continuous_block_run(disk->data_bitmap, 1, current_byte,
			&run_start_bit);

		if (r != SUCCESS) return r;

		r = bitmap_find_continuous_run_length(disk->data_bitmap, run_start_bit, &length);
		current_byte = run_start_bit / 8 + length / 8;
		allocated_bytes += length * BLOCK_SIZE;

		// Write to the copied bitmap
		for (int i = 0; i < length; i++) {
			bitmap_write(&disk->data_bitmap, i + run_start_bit, 1);
		}

		disk->superblock.num_used_blocks += length;

		// Add allocation to LL
		BlockSequence* seq = malloc(sizeof(BlockSequence));
		seq->length = length;
		seq->start_addr = run_start_bit;
		llist_insert(*addresses, seq);

		disk->superblock.data_bitmap_circular_loc = run_start_bit / 8;
	}

	return SUCCESS;
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
			bitmap_write_range(disk->data_bitmap, start_bit, num_blocks, 1);
			sb->num_used_blocks += num_blocks;
		}

	}

	if (ret == ERR_NO_BITMAP_RUN_FOUND || ft_ratio >= ALLOC_FULL_FT_MAX) {
		_fs_allocate_fragmented(disk, num_blocks, addresses);
	}

	return SUCCESS;
}

int fs_write_data_to_disk(Disk* disk, HeapData data, LList addresses, bool data_block) {
	if (disk->file == NULL) return ERR_INVALID_FILE_OPERATION;
	if (addresses.num_elements == 0) return ERR_TOO_FEW_ADDRESSES_PROVIDED;
	if (!data.valid) return ERR_INVALID_MEMORY_ACCESS;

	int ret = 0;
	int blocks_written = 0;
	LListNode* current = addresses.head;
	for (int i = 0; i < addresses.num_elements; i++) {
		BlockSequence* seq = current->element;

		HeapData section = { 0 };
		ret = mem_alloc(&section, seq->length * BLOCK_SIZE);
		if (ret != SUCCESS) return ret;

		int bytes_from_data = 0;
		if(data.size < (BLOCK_SIZE * seq->length + BLOCK_SIZE * blocks_written)) {
			bytes_from_data = data.size - BLOCK_SIZE * blocks_written;
		} else {
			bytes_from_data = BLOCK_SIZE * seq->length;
		}

		memcpy(section.data, &data.data[blocks_written * BLOCK_SIZE], bytes_from_data);
		if (!data_block) {
			ret = disk_write(disk, seq->start_addr * BLOCK_SIZE, section);
		}
		else {
			int offset = disk->superblock.data_blocks_start_addr * BLOCK_SIZE;
			disk_write_offset(disk, seq->start_addr * BLOCK_SIZE, offset, section);
		}
		if (ret != SUCCESS) return ret;


		blocks_written += seq->length;
		mem_free(section);
		current = current->next;
	}

	return SUCCESS;
}

HeapData fs_read_from_disk_by_sequence(Disk disk, BlockSequence seq, bool data_block, int* error) {
	const int location = seq.start_addr * BLOCK_SIZE;
	const int size = seq.length * BLOCK_SIZE;

	int ret = 0;
	if (data_block) {
		const int offset = disk.superblock.data_blocks_start_addr * BLOCK_SIZE;
		HeapData data = disk_read_offset(disk, location, offset, size, &ret);
		return data;
	}
	else {
		HeapData data = disk_read(disk, location, size, &ret);
		return data;
	}
}
HeapData fs_read_from_disk(Disk disk, LList addresses, bool data_block, int* error) {
	HeapData data = {0};

	if(addresses.num_elements <= 0) {
		*error = SUCCESS;		// Should this return a non zero value?
		return data;
	}

	LListNode* current = addresses.head;
	for(int i = 0; i < addresses.num_elements; i++) {
		BlockSequence* seq = current->element;
		HeapData read = fs_read_from_disk_by_sequence(disk, *seq, data_block, error);
		if(*error != SUCCESS) return data;
		int read_mem_start = 0;

		// Add read data to data
		if(!data.valid) {
			mem_alloc(&data, read.size);
			memcpy(data.data, read.data, read.size);
		} else {
			read_mem_start = data.size;
			mem_realloc(&data, data.size + read.size);
			memcpy(&data.data[read_mem_start], read.data, read.size);
		}

		current = current->next;
	}

	return data;
}