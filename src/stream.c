#include "stream.h"
#include "util.h"
#include <string.h>
#include <math.h>

int _stream_write_seq_to_heap(BlockSequence seq, HeapData* data, int location) {
	int ret = util_write_uint32(data, location, seq.start_addr);
	if (ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location + 4, seq.length);
	if (ret != SUCCESS) return ret;

	return SUCCESS;
}

int _stream_write_address_level(Disk disk, BlockSequence* inode_data, LList addresses, LList** next_addresses, HeapData* next_data) {
	// Alloc memory for serialized addresses
	int ret = mem_alloc(next_data, addresses.num_elements * BLOCK_SEQ_SIZE);
	if (ret != SUCCESS) return ret;

	// Serialize addresses
	LListNode* current = addresses.head;
	for (int i = 0; i < addresses.num_elements; i++) {
		BlockSequence* seq = current->element;
		_stream_write_seq_to_heap(*seq, next_data, i * BLOCK_SEQ_SIZE);
		current = current->next;
	}

	// Allocate blocks for the serialized data
	ret = fs_allocate_blocks(&disk, div_round_up(next_data->size, BLOCK_SIZE), next_addresses);
	if (ret != SUCCESS) return ret;

	// Set inode information
	if ((*next_addresses)->num_elements > 0) {
		BlockSequence* seq = (*next_addresses)->head->element;
		*inode_data = *seq;
	}

	return SUCCESS;
}

int stream_write_addresses(Disk* disk, Inode* inode, LList addresses){
	int ret = SUCCESS;

	// Write directs to the inode
	LListNode* current = addresses.head;
	int num_directs = addresses.num_elements < DIRECT_BLOCK_NUM ? addresses.num_elements : DIRECT_BLOCK_NUM;

	for (int i = 0; i < num_directs; i++) {
		BlockSequence* seq = current->element;
		inode->data.direct[i] = *seq;
		current = current->next;
	}

	if(num_directs < DIRECT_BLOCK_NUM) return SUCCESS;

	// Split up into direct and indirects 
	LList* indirect_addresses = llist_create_sublist(addresses, DIRECT_BLOCK_NUM, &ret);
	if (indirect_addresses->num_elements == 0) {
		return SUCCESS;
	}

	HeapData indirect_data = { 0 };
	HeapData double_indirect_data = { 0 };
	HeapData triple_indirect_data = { 0 };

	LList* double_indirect_addresses;
	LList* indirect_data_addresses;
	LList* triple_indirect_addresses;


	_stream_write_address_level(*disk, &inode->data.indirect, *indirect_addresses, &indirect_data_addresses, &indirect_data);
	ret = fs_write_data_to_disk(disk, indirect_data, *indirect_data_addresses, true);
	if (ret != SUCCESS) return ret;

	LList* remaining_indirects = llist_create_sublist(*indirect_data_addresses, 1, &ret);
	if (ret != SUCCESS) return ret;

	if (remaining_indirects->num_elements == 0) {
		return SUCCESS;
	}


	_stream_write_address_level(*disk, &inode->data.double_indirect, *remaining_indirects, &double_indirect_addresses, &double_indirect_data);
	ret = fs_write_data_to_disk(disk, double_indirect_data, *double_indirect_addresses, true);
	if (ret != SUCCESS) return ret;

	LList* remaining_double_indirects = llist_create_sublist(*double_indirect_addresses, 1, &ret);
	if (ret != SUCCESS) return ret;

	if (remaining_double_indirects->num_elements == 0) {
		return SUCCESS;
	}

	_stream_write_address_level(*disk, &inode->data.triple_indirect, *remaining_double_indirects, &triple_indirect_addresses, &triple_indirect_data);
	if (triple_indirect_addresses->num_elements > 1) {
		return ERR_DISK_FULL;
	}
	ret = fs_write_data_to_disk(disk, triple_indirect_data, *triple_indirect_addresses, true);
	if (ret != SUCCESS) return ret;

	return SUCCESS;
}

LList* stream_read_address_block(Disk disk, BlockSequence block, int* error) {
	int ret = 0;
	LList* addresses = llist_new();
	addresses->free_element = &free_element_standard;


	HeapData data = fs_read_from_disk_by_sequence(disk, block, true, &ret);
	for (int i = 0; i < data.size; i += BLOCK_SEQ_SIZE) {
		BlockSequence* seq = malloc(sizeof(BlockSequence));
		
		seq->start_addr = util_read_uint32(data, i, &ret);
		if (ret != SUCCESS) {
			*error = ret;
			return addresses;
		}

		seq->length = util_read_uint32(data, i + 4, &ret);
		if (ret != SUCCESS) {
			*error = ret;
			return addresses;
		}

		if(block_seq_is_empty(*seq)) {
			break;
		}

		llist_insert(addresses, seq);
	}

	return addresses;
}

int stream_double_to_block_seq(Disk disk, BlockSequence double_indirect, LList** addresses) {
	*addresses = llist_new();
	(*addresses)->free_element = &free_element_standard;
	int ret = 0;

	// Double indirect -> indirect addresses
	LList* indirect_addresses = stream_read_address_block(disk, double_indirect, &ret);

	// For each indirect addresses, indirect -> block sequences
	LListNode* current = indirect_addresses->head;
	for (int i = 0; i < indirect_addresses->num_elements; i++) {
		BlockSequence* indirect_addr = current->element;
		if (block_seq_is_empty(*indirect_addr)) break;

		LList* indirects = stream_read_address_block(disk, *indirect_addr, &ret);
		if (ret != SUCCESS) return ret;

		llist_append(*addresses, *indirects);
		free(indirects);	// TODO fix this
		current = current->next;
	}

	return SUCCESS; 
}

int stream_triple_to_block_seq(Disk disk, BlockSequence triple_indirect, LList** addresses) {
	*addresses = llist_new();
	(*addresses)->free_element = &free_element_standard;
	int ret = 0;

	// triple indirect -> double indirect addresses
	LList* double_indirect_addresses = stream_read_address_block(disk, triple_indirect, &ret);

	// For each double indirect addresses, double -> block sequences
	LListNode* current = double_indirect_addresses->head;
	for (int i = 0; i < double_indirect_addresses->num_elements; i++) {
		BlockSequence* double_indirect_addr = current->element;
		if (block_seq_is_empty(*double_indirect_addr)) break;

		LList* block_sequences = llist_new();
		block_sequences->free_element = &free_element_standard;

		ret = stream_double_to_block_seq(disk, *double_indirect_addr, &block_sequences);
		if (ret != SUCCESS) return ret;

		ret = llist_append(*addresses, *block_sequences);
		if (ret != SUCCESS) return ret;
		free(block_sequences);
		current = current->next;
	}

	return SUCCESS;
}

LList stream_read_addresses(Disk disk, Inode inode, int* error) {
	int ret = 0;
	LList* addresses = llist_new();
	addresses->free_element = &free_element_standard;

	// Add directs
	for (int i = 0; i < DIRECT_BLOCK_NUM; i++) {
		if(block_seq_is_empty(inode.data.direct[i])) {
			*error = SUCCESS;
			return *addresses;
		}

		ret = llist_insert(addresses, &inode.data.direct[i]);
		if (ret != SUCCESS) {
			*error = ret;
			return *addresses;
		}
	}

	if (block_seq_is_empty(inode.data.indirect)) {
		*error = SUCCESS;
		return *addresses;
	}

	LList* indirect_sequences = stream_read_address_block(disk, inode.data.indirect, &ret);
	if (ret != SUCCESS) {
		*error = ret;
		return *addresses;
	}

	if (block_seq_is_empty(inode.data.double_indirect)) {
		*error = SUCCESS;
		return *addresses;
	}


	LList* double_indirect_sequences;
	ret = stream_double_to_block_seq(disk, inode.data.double_indirect, &double_indirect_sequences);
	if (ret != SUCCESS) {
		*error = ret;
		return *addresses;
	}

	if (block_seq_is_empty(inode.data.triple_indirect)) {
		*error = SUCCESS;
		return *addresses;
	}


	LList* triple_indirect_sequences;
	ret = stream_triple_to_block_seq(disk, inode.data.triple_indirect, &triple_indirect_sequences);
	if (ret != SUCCESS) {
		*error = ret;
		return *addresses;
	}

	// TODO fix error codes with llist
	llist_append(addresses, *indirect_sequences);
	llist_append(addresses, *double_indirect_sequences);
	llist_append(addresses, *triple_indirect_sequences);
	
	free(indirect_sequences);
	free(double_indirect_sequences);
	free(triple_indirect_sequences);

	return *addresses;
}