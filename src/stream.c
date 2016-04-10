#include "stream.h"
#include "util.h"
#include <string.h>
#include "memory.h"
#include "constants.h"

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

	// Append on a zero block sequence to the de serailization functino
	// (stream_read_addresses) knows when to stop reading (otherwise will
	// just start reading garbage).
	BlockSequence* zero = malloc(sizeof(zero));
	memset(zero, 0, sizeof(BlockSequence));
	llist_insert(&addresses, zero);

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
		llist_free(remaining_indirects);
		mem_free(indirect_data);
		return SUCCESS;
	}


	_stream_write_address_level(*disk, &inode->data.double_indirect, *remaining_indirects, &double_indirect_addresses, &double_indirect_data);
	ret = fs_write_data_to_disk(disk, double_indirect_data, *double_indirect_addresses, true);
	if (ret != SUCCESS) return ret;

	LList* remaining_double_indirects = llist_create_sublist(*double_indirect_addresses, 1, &ret);
	if (ret != SUCCESS) return ret;

	if (remaining_double_indirects->num_elements == 0) {
		llist_free(remaining_double_indirects);
		mem_free(double_indirect_data);
		mem_free(indirect_data);
		return SUCCESS;
	}

	_stream_write_address_level(*disk, &inode->data.triple_indirect, *remaining_double_indirects, &triple_indirect_addresses, &triple_indirect_data);
	if (triple_indirect_addresses->num_elements > 2) {
		// Note that this is not 1 - we add a zero block sequence at the beginning.
		return ERR_DISK_FULL;
	}
	ret = fs_write_data_to_disk(disk, triple_indirect_data, *triple_indirect_addresses, true);
	if (ret != SUCCESS) return ret;

	mem_free(triple_indirect_data);
	mem_free(double_indirect_data);
	mem_free(indirect_data);

	llist_free(remaining_double_indirects);
	llist_free(remaining_indirects);

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
	mem_free(data);

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

		append_block_sequence_lists(*addresses, *indirects);
		llist_free(indirects);	// TODO fix this
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

		append_block_sequence_lists(*addresses, *block_sequences);
		llist_free(block_sequences);
		current = current->next;
	}

	return SUCCESS;
}

LList* stream_read_addresses(Disk disk, Inode inode, int* error) {
	int ret = 0;
	LList* addresses = llist_new();
	addresses->free_element = &free_element_bl_debug;


	// Add directs
	for (int i = 0; i < DIRECT_BLOCK_NUM; i++) {
		if(block_seq_is_empty(inode.data.direct[i])) {
			*error = SUCCESS;
			return addresses;
		}

		ret = llist_insert(addresses, &inode.data.direct[i]);
		if (ret != SUCCESS) {
			*error = ret;
			return addresses;
		}
	}

	if (block_seq_is_empty(inode.data.indirect)) {
		*error = SUCCESS;
		return addresses;
	}

	LList* indirect_sequences = stream_read_address_block(disk, inode.data.indirect, &ret);
	if (ret != SUCCESS) {
		*error = ret;
		return addresses;
	}

	append_block_sequence_lists(addresses, *indirect_sequences);


	if (block_seq_is_empty(inode.data.double_indirect)) {
		llist_free(indirect_sequences);
		*error = SUCCESS;
		return addresses;
	}


	LList* double_indirect_sequences;
	ret = stream_double_to_block_seq(disk, inode.data.double_indirect, &double_indirect_sequences);
	if (ret != SUCCESS) {
		*error = ret;
		return addresses;
	}

	append_block_sequence_lists(addresses, *double_indirect_sequences);

	if (block_seq_is_empty(inode.data.triple_indirect)) {
		llist_free(indirect_sequences);
		llist_free(double_indirect_sequences);
		*error = SUCCESS;
		return addresses;
	}


	LList* triple_indirect_sequences;
	ret = stream_triple_to_block_seq(disk, inode.data.triple_indirect, &triple_indirect_sequences);
	if (ret != SUCCESS) {
		*error = ret;
		return addresses;
	}

	append_block_sequence_lists(addresses, *triple_indirect_sequences);

	llist_free(indirect_sequences);
	llist_free(double_indirect_sequences);
	llist_free(triple_indirect_sequences);

	return addresses;
}

LList* stream_read_alloc_idts(Disk disk, Inode inode, int* error) {
	LList* addresses = llist_new();
	addresses->free_element = &free_element_standard;

	int ret = 0;

	if(block_seq_is_empty(inode.data.double_indirect)) {
		return addresses;
	}
	
	BlockSequence* indirect = malloc(sizeof(BlockSequence));
	memcpy(indirect, &inode.data.double_indirect, sizeof(BlockSequence));
	llist_insert(addresses, indirect);

	LList* double_indirects = stream_read_address_block(disk, inode.data.double_indirect, &ret);
	if(ret != SUCCESS) {
		*error = ret;
		return addresses;
	}

	append_block_sequence_lists(addresses, *double_indirects);

	if(block_seq_is_empty(inode.data.triple_indirect)) {
		return addresses;
	}

	BlockSequence* t_indirect = malloc(sizeof(BlockSequence));
	memcpy(indirect, &inode.data.triple_indirect, sizeof(BlockSequence));
	llist_insert(addresses, t_indirect);


	LList* triple_indirects = stream_read_address_block(disk, inode.data.triple_indirect, &ret);
	if(ret != SUCCESS) {
		*error = ret;
		return addresses;
	}

	append_block_sequence_lists(addresses, *triple_indirects);


	// Now read indirects from T indirects (D indirects)
	LListNode* current = triple_indirects->head;
	for(int i = 0; i < triple_indirects->num_elements; i++) {
		BlockSequence* seq = current->element;

		LList* t_double_indirects = stream_read_address_block(disk, *seq, &ret);
		if(ret != SUCCESS) {
			*error = ret;
			return addresses;
		}

		if(t_double_indirects->num_elements == 0) break;

		// Write to main LList
		append_block_sequence_lists(addresses, *t_double_indirects);
		current = current->next;
	}


	return addresses;
}

int stream_append_to_addresses(Disk disk, Inode* inode, LList new_addresses) {
	// Read existing addresses
	int ret = 0;
	LList* addresses = stream_read_alloc_idts(disk, *inode, &ret);

	ret = stream_clear_bitmap(&disk, addresses);
	if(ret != SUCCESS) return ret;

	// Create new list 
	LList* block_addresses = stream_read_addresses(disk, *inode, &ret);
	if(ret != SUCCESS) return ret;

	append_block_sequence_lists(block_addresses, new_addresses);

	BlockSequence* zero = malloc(sizeof(zero));
	memset(zero, 0, sizeof(BlockSequence));
	llist_insert(block_addresses, zero);
	
	// Write addresses 
	ret = stream_write_addresses(&disk, inode, *block_addresses);
	mem_dump(disk.data_bitmap, "after.bin");

	free(zero);
	// TODO llist_free(addresses);	
	return SUCCESS;
}

int stream_clear_bitmap(Disk* disk, LList* addresses) {
	LListNode* current = addresses->head;
	int ret;

	for(int i = 0; i < addresses->num_elements; i++) {
		BlockSequence* seq = current->element;

		ret = bitmap_write_range(disk->data_bitmap, seq->start_addr, seq->length, 0);
		if(ret != SUCCESS) return ret;

		current = current->next;
	}

	return SUCCESS;
}