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
#include "serialize.h"
#include "directory.h"
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
		if(r != SUCCESS) return r;

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

	int ret;
	int blocks_written = 0;
	LListNode* current = addresses.head;
	for (int i = 0; i < addresses.num_elements; i++) {
		BlockSequence* seq = current->element;

		HeapData section = { 0 };
		ret = mem_alloc(&section, seq->length * BLOCK_SIZE);
		if (ret != SUCCESS) return ret;

		int bytes_from_data;
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
		int read_mem_start;

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

int fs_write_inode(Disk disk, Inode *inode, int* inode_number) {
	// Find free inode on disk
	const int start = 0;	// TODO set this correctly
	int block_addr = 0;
	int ret;

	ret = bitmap_find_block(disk.inode_bitmap, start, &block_addr);
	if(ret != SUCCESS) return ret;

	*inode_number = block_addr;
	inode->inode_number = block_addr;

	ret = fs_write_inode_data(disk, *inode, block_addr);
	if(ret != SUCCESS) return ret;

	// Update bitmap
	bitmap_write(&disk.inode_bitmap, block_addr, 1);
	disk.superblock.num_used_inodes += 1;

	return SUCCESS;
}

int fs_write_inode_data(Disk disk, Inode inode, int inode_num) {
	// Serialize inode
	inode.inode_number = inode_num;
	HeapData inode_data = {0};
	mem_alloc(&inode_data, INODE_SIZE);
	int ret = serialize_inode(&inode_data, inode);

	// Write inode to disk
	double disk_offset = inode_addr_to_disk_block_addr(disk, inode_num);

	ret = disk_write(&disk, disk_offset * BLOCK_SIZE, inode_data);
	if(ret != SUCCESS) return ret;
	return SUCCESS;
}

int fs_write_file(Disk* disk, Inode* inode, HeapData data, int* inode_number) {
	/*
		1) Allocate space on the disk to store the data
		2) Write the disk data to the disk
		3) Write the addresses to the inode and disk
		4) Write the inode to the disk
	*/

	int ret;
	
	LList* addresses;
	ret = fs_allocate_blocks(disk, div_round_up(data.size, BLOCK_SIZE), &addresses);
	if(ret != SUCCESS) return ret;

	ret = fs_write_data_to_disk(disk, data, *addresses, true);
	if(ret != SUCCESS) return ret;

	ret = stream_write_addresses(disk, inode, *addresses);
	if(ret != SUCCESS) return ret;

	int inode_num = 0;
	ret = fs_write_inode(*disk, inode, &inode_num);
	if(ret != SUCCESS) return ret;

	*inode_number = inode_num;
	inode->size = data.size;
	return SUCCESS;
} 

int fs_delete_file(Disk* disk, HeapData path) {
	int ret = 0;
	Inode inode = fs_get_inode_from_path(*disk, path, &ret);
	if(ret != SUCCESS) return ret;

	LList* addresses = stream_read_addresses(*disk, inode, &ret);
	if(ret != SUCCESS) return ret;

	ret = stream_clear_bitmap(disk, addresses);
	if(ret != SUCCESS) return ret;

	ret = bitmap_write(&disk->data_bitmap, inode.inode_number, 0);
	if(ret != SUCCESS) return ret;

	return SUCCESS;
}

Inode fs_read_inode(Disk disk, int inode_num, int* error) {
	Inode inode = {0};
	int ret = 0;

	HeapData inode_data = fs_read_inode_data(disk, inode_num, &ret);
	if(ret != SUCCESS) {
		*error = ret;
		return inode;
	}

	ret = unserialize_inode(&inode_data, &inode);
	if(ret != SUCCESS) {
		*error = ret;
		return inode;
	}

	inode.inode_number = inode_num;

	return inode;
}

HeapData fs_read_inode_data(Disk disk, int inode_num, int* error) {
	int ret = 0;
	HeapData inode_data;

	double block_addr = inode_addr_to_disk_block_addr(disk, inode_num);
	inode_data = disk_read(disk, block_addr * BLOCK_SIZE, INODE_SIZE, &ret);
	if(ret != SUCCESS) {
		*error = ret;
		return inode_data;
	}

	return inode_data;

}

Disk fs_create_filesystem(const char* name, int size, int* error) {
	Disk disk = {0};
	Superblock sb = {0};
	Bitmap data_bt = {0};
	Bitmap inode_bt = {0};

	disk.superblock = sb;
	disk.data_bitmap = data_bt;
	disk.inode_bitmap = inode_bt;
	disk.size = size;

	int ret = fs_create_superblock(&disk.superblock, size);
	if(ret != SUCCESS) {
		*error = ret;
		return disk;
	}

	ret = disk_mount(&disk, name);
	if(ret != SUCCESS) {
		*error = ret;
		return disk;
	}

	ret = mem_alloc(&disk.data_bitmap, disk.superblock.data_block_bitmap_size_bytes);
	if(ret != SUCCESS) {
		*error = ret;
		return disk;
	}

	ret = mem_alloc(&disk.inode_bitmap, disk.superblock.inode_bitmap_size_bytes);
	if(ret != SUCCESS) {
		*error = ret;
		return disk;
	}

	disk.filename = name;

	return disk;
}

int fs_write_metadata(Disk disk) {
	HeapData superblock = {0};
	mem_alloc(&superblock, BLOCK_SIZE);
	serialize_superblock(&superblock, disk.superblock);

	int ret = disk_write(&disk, SUPERBLOCK_BLOCK_ADDR * BLOCK_SIZE, superblock);
	if(ret != SUCCESS) return ret;

	ret = disk_write(&disk, INODE_BITMAP_BLOCK_ADDR * BLOCK_SIZE, disk.inode_bitmap);
	if(ret != SUCCESS) return ret;

	ret = disk_write(&disk, disk.superblock.data_block_bitmap_addr * BLOCK_SIZE, disk.data_bitmap);
	if(ret != SUCCESS) return ret;

	return SUCCESS;
}

int fs_read_metadata(Disk* disk) {
	int ret = 0;
	
	HeapData superblock = disk_read(*disk, SUPERBLOCK_BLOCK_ADDR * BLOCK_SIZE, BLOCK_SIZE, &ret);
	if(ret != SUCCESS) return ret;
	ret = unserialize_superblock(&superblock, &disk->superblock);
	if(ret != SUCCESS) return ret;

	if(disk->superblock.magic_1 != SUPERBLOCK_MAGIC_1) return ERR_INCORRECT_SUPERBLOCK_MAGIC;
	if(disk->superblock.magic_2 != SUPERBLOCK_MAGIC_2) return ERR_INCORRECT_SUPERBLOCK_MAGIC;

	disk->inode_bitmap = disk_read(*disk, INODE_BITMAP_BLOCK_ADDR, disk->superblock.inode_bitmap_size, &ret);
	if(ret != SUCCESS) return ret;

	disk->data_bitmap = disk_read(*disk, disk->superblock.data_blocks_start_addr * BLOCK_SIZE, disk->superblock.data_block_bitmap_size, &ret);
	if(ret != SUCCESS) return ret;

	return SUCCESS;
}

int fs_fill_unused_allocated_data(Disk* disk, Inode* inode, HeapData new_data, HeapData* remaining_data) {
	int ret = 0;
	
	const int num_full_blocks = inode->size / BLOCK_SIZE;
	const int data_end_bytes = inode->size - num_full_blocks * BLOCK_SIZE;
	int free_bytes = BLOCK_SIZE - data_end_bytes;
	HeapData remaining = {0};

	// Find the space on disk
	LList* addresses = stream_read_addresses(*disk, *inode, &ret);
	if(ret != SUCCESS) return ret;

	if(addresses->num_elements != 0) {
		BlockSequence* seq = addresses->tail->element;
		const int byte_offset = seq->start_addr * BLOCK_SIZE + data_end_bytes;

		const int actual_size = new_data.size;
		new_data.size = (free_bytes < new_data.size) ? free_bytes : new_data.size;

		// Write data
		ret = disk_write_offset(disk, byte_offset, disk->superblock.data_blocks_start_addr * BLOCK_SIZE, new_data);
		if(ret != SUCCESS) return ret;

		//inode->size += new_data.size;
		new_data.size = actual_size;

		if(free_bytes > new_data.size) {
			*remaining_data = remaining;
			return SUCCESS;
		} 
	} else {
		free_bytes = 0;
	}

	ret = mem_alloc(&remaining, new_data.size - free_bytes);
	if(ret != SUCCESS) return ret;
	
	memcpy(remaining.data, &new_data.data[free_bytes], remaining.size);
	*remaining_data = remaining;

	return SUCCESS;
}

int fs_write_to_file(Disk* disk, int inode_number, HeapData data) {
	int ret = 0;

	Inode inode =  fs_read_inode(*disk, inode_number, &ret);
	if(ret != SUCCESS) return ret;

	HeapData remaining = {0};
	ret = fs_fill_unused_allocated_data(disk, &inode, data, &remaining);
	if(ret != SUCCESS) return ret;

	if(remaining.size == 0) {
		inode.size += data.size;
		ret = fs_write_inode_data(*disk, inode, inode.inode_number);
		if(ret != SUCCESS) return ret;

		return SUCCESS;
	}
	
	LList* addresses;
	ret = fs_allocate_blocks(disk, div_round_up(remaining.size, BLOCK_SIZE), &addresses);
	if(ret != SUCCESS) return ret;
	
	ret = fs_write_data_to_disk(disk, remaining, *addresses, true);
	if(ret != SUCCESS) return ret;

	ret = stream_write_addresses(disk, &inode, *addresses);
	if(ret != SUCCESS) return ret;

	inode.size += data.size;

	fs_write_inode_data(*disk, inode, inode.inode_number);
	ret = stream_write_addresses(disk, &inode, *addresses);
	if(ret != SUCCESS) return ret;

	return SUCCESS;
}

Inode fs_get_inode_from_path(Disk disk, HeapData path, int* error) {
	Inode inode = {0};
	DirectoryEntry file;

	int ret = 0;

	Inode root_inode = fs_read_inode(disk, ROOT_DIRECTORY_INODE_NUMBER, &ret);
	if(ret != SUCCESS) {
		*error = ret;
		return inode;
	}

	LList* root_addresses = stream_read_addresses(disk, root_inode, &ret);
	if(ret != SUCCESS) {
		*error = ret;
		return inode;
	}
	Directory root = fs_read_from_disk(disk, *root_addresses, true, &ret);
	if(ret != SUCCESS) {
		*error = ret;
		return inode;
	}
	ret = dir_get_directory(disk, path, root, &file);
	if(ret != SUCCESS) {
		*error = ret;
		return inode;
	}
	inode = fs_read_inode(disk, file.inode_number, &ret);
	if(ret != SUCCESS) {
		*error = ret;
		return inode;
	}

	return inode;
}
