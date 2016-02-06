#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "constants.h"
#include "memory.h"
#include "fs.h"
#include "util.h"



int serialize_superblock(HeapData* data, Superblock superblock){
	// Using constants to update location_count to help prevent mistakes
	const int INCREMENT_16 = 2;
	const int INCREMENT_32 = 4;

	int location_count = 0;

	int ret = util_write_uint32(data, location_count, superblock.magic_1);
	location_count += INCREMENT_32;
	if(ret != SUCCESS) return ret;

	ret = util_write_uint16(data, location_count, superblock.version);
	location_count += INCREMENT_16;
	if(ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location_count, superblock.block_size);
	location_count += INCREMENT_32;
	if(ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location_count, superblock.num_blocks);
	location_count += INCREMENT_32;
	if(ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location_count, superblock.num_used_blocks);
	location_count += INCREMENT_32;
	if(ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location_count, superblock.num_data_blocks);
	location_count += INCREMENT_32;
	if(ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location_count, superblock.inode_size);
	location_count += INCREMENT_32;
	if(ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location_count, superblock.num_inodes);
	location_count += INCREMENT_32;
	if(ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location_count, superblock.num_used_inodes);
	location_count += INCREMENT_32;
	if(ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location_count, superblock.magic_2);
	location_count += INCREMENT_32;
	if(ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location_count, superblock.inode_bitmap_size);
	location_count += INCREMENT_32;
	if(ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location_count, superblock.inode_table_size);
	location_count += INCREMENT_32;
	if(ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location_count, superblock.data_block_bitmap_size);
	location_count += INCREMENT_32;
	if(ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location_count, superblock.inode_table_start_addr);
	location_count += INCREMENT_32;
	if(ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location_count, superblock.data_block_bitmap_addr);
	location_count += INCREMENT_32;
	if(ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location_count, superblock.data_blocks_start_addr);
	location_count += INCREMENT_32;
	if(ret != SUCCESS) return ret;
	
	ret = util_write_uint16(data, location_count, superblock.flags);
	location_count += INCREMENT_16;
	if(ret != SUCCESS) return ret;

	return SUCCESS;
}

int unserialize_superblock(HeapData* data, Superblock* superblock){
	int error = 0;
	int location_count = 0;

	const int INCREMENT_16 = 2;
	const int INCREMENT_32 = 4;

	superblock->magic_1 = util_read_uint32(*data, location_count, &error);
	location_count += INCREMENT_32;
	if (error != SUCCESS) return error;

	superblock->version = util_read_uint16(*data, location_count, &error);
	location_count += INCREMENT_16;
	if (error != SUCCESS) return error;

	superblock->block_size = util_read_uint32(*data, location_count, &error);
	location_count += INCREMENT_32;
	if (error != SUCCESS) return error;

	superblock->num_blocks = util_read_uint32(*data, location_count, &error);
	location_count += INCREMENT_32;
	if (error != SUCCESS) return error;

	superblock->num_used_blocks = util_read_uint32(*data, location_count, &error);
	location_count += INCREMENT_32;
	if (error != SUCCESS) return error;

	superblock->num_data_blocks = util_read_uint32(*data, location_count, &error);
	location_count += INCREMENT_32;
	if (error != SUCCESS) return error;

	superblock->inode_size = util_read_uint32(*data, location_count, &error);
	location_count += INCREMENT_32;
	if (error != SUCCESS) return error;

	superblock->num_inodes = util_read_uint32(*data, location_count, &error);
	location_count += INCREMENT_32;
	if (error != SUCCESS) return error;

	superblock->num_used_inodes = util_read_uint32(*data, location_count, &error);
	location_count += INCREMENT_32;
	if (error != SUCCESS) return error;

	superblock->magic_2 = util_read_uint32(*data, location_count, &error);
	location_count += INCREMENT_32;
	if (error != SUCCESS) return error;

	superblock->inode_bitmap_size = util_read_uint32(*data, location_count, &error);
	location_count += INCREMENT_32;
	if (error != SUCCESS) return error;

	superblock->inode_table_size = util_read_uint32(*data, location_count, &error);
	location_count += INCREMENT_32;
	if (error != SUCCESS) return error;

	superblock->data_block_bitmap_size = util_read_uint32(*data, location_count, &error);
	location_count += INCREMENT_32;
	if (error != SUCCESS) return error;

	superblock->inode_table_start_addr = util_read_uint32(*data, location_count, &error);
	location_count += INCREMENT_32;
	if (error != SUCCESS) return error;

	superblock->data_block_bitmap_addr = util_read_uint32(*data, location_count, &error);
	location_count += INCREMENT_32;
	if (error != SUCCESS) return error;

	superblock->data_blocks_start_addr = util_read_uint32(*data, location_count, &error);
	location_count += INCREMENT_32;
	if (error != SUCCESS) return error;

	superblock->flags = util_read_uint16(*data, location_count, &error);
	location_count += INCREMENT_16;
	if (error != SUCCESS) return error;


	return SUCCESS;
}

int serialize_inode(HeapData* data, Inode inode) {
	int error = 0;
	int location_count = 0;
	const int INCREMENT_32 = 4;
	const int INCREMENT_64 = 8;

	error = util_write_uint32(data, location_count, inode.magic);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	error = util_write_uint32(data, location_count, inode.inode_number);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	error = util_write_uint32(data, location_count, inode.uid);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	error = util_write_uint32(data, location_count, inode.gid);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	error = util_write_uint32(data, location_count, inode.flags);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	error = util_write_uint32(data, location_count, inode.time_created);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	error = util_write_uint32(data, location_count, inode.time_last_modified);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	error = util_write_uint64(data, location_count, inode.size);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_64;
	
	error = util_write_uint32(data, location_count, inode.preallocation_size);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	error = util_write_uint32(data, location_count, inode.data.indirect);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	error = util_write_uint32(data, location_count, inode.data.double_indirect);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	error = util_write_uint32(data, location_count, inode.data.triple_indirect);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	for (int i = 0; i < DIRECT_BLOCK_NUM; i++) {
		error = util_write_uint32(data, location_count, inode.data.direct[i].start_addr);
		if (error != SUCCESS) return error;
		location_count += INCREMENT_32;

		error = util_write_uint32(data, location_count, inode.data.direct[i].length);
		if (error != SUCCESS) return error;
		location_count += INCREMENT_32;
	}


	return SUCCESS;
}

int unserialize_inode(HeapData* data, Inode* inode) {
	int error = 0;
	int location_count = 0;
	const int INCREMENT_32 = 4;
	const int INCREMENT_64 = 8;

	inode->magic = util_read_uint32(*data, location_count, &error);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	inode->inode_number = util_read_uint32(*data, location_count, &error);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	inode->uid = util_read_uint32(*data, location_count, &error);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	inode->gid = util_read_uint32(*data, location_count, &error);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	inode->flags = util_read_uint32(*data, location_count, &error);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	inode->time_created = util_read_uint32(*data, location_count, &error);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	inode->time_last_modified = util_read_uint32(*data, location_count, &error);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;
	
	inode->size = util_read_uint64(*data, location_count, &error);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_64;

	inode->preallocation_size = util_read_uint32(*data, location_count, &error);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	inode->data.indirect = util_read_uint32(*data, location_count, &error);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	inode->data.double_indirect = util_read_uint32(*data, location_count, &error);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	inode->data.triple_indirect = util_read_uint32(*data, location_count, &error);
	if (error != SUCCESS) return error;
	location_count += INCREMENT_32;

	for (int i = 0; i < DIRECT_BLOCK_NUM; i++) {
		inode->data.direct[i].start_addr = util_read_uint32(*data, location_count, &error);
		if (error != SUCCESS) return error;
		location_count += INCREMENT_32;

		inode->data.direct[i].length = util_read_uint32(*data, location_count, &error);
		if (error != SUCCESS) return error;
		location_count += INCREMENT_32;
	}

	return SUCCESS;
}
