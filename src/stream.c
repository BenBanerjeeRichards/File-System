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
int stream_write_addresses_to_heap(LList addresses, HeapData* data) {
	LListNode* current = addresses.head;
	int ret = 0;

	for (int i = 0; i < addresses.num_elements; i++) {
		BlockSequence* seq = current->element;
		ret = _stream_write_seq_to_heap(*seq, data, i * BLOCK_SEQ_SIZE);
		if (ret != SUCCESS) return ret;
		current = current->next;
	}

	return SUCCESS;
}

int stream_write_addresses(Disk* disk, Inode* inode, LList addresses){
	int ret = SUCCESS;
	// Split up into direct and indirects 
	LList* indirect_addresses = llist_create_sublist(addresses, DIRECT_BLOCK_NUM, &ret);

	// Write directs to the inode
	LListNode* current = addresses.head;
	int num_directs = addresses.num_elements < 6 ? addresses.num_elements : 6;
	for (int i = 0; i < num_directs; i++) {
		BlockSequence* seq = current->element;
		inode->data.direct[i] = *seq;
		current = current->next;
	}

	if (indirect_addresses->num_elements == 0) {
		/** DONE **/
	}

	HeapData indirect_data = { 0 };
	ret = mem_alloc(&indirect_data, indirect_addresses->num_elements * BLOCK_SEQ_SIZE);
	if (ret != SUCCESS) return ret;

	// Writing block sequences to indirects
	current = indirect_addresses->head;
	for (int i = 0; i < indirect_addresses->num_elements; i++) {
		BlockSequence* seq = current->element;
		_stream_write_seq_to_heap(*seq, &indirect_data, i * BLOCK_SEQ_SIZE);
		current = current->next;
	}

	// Find addresses to store indirects
	LList* indirect_data_addresses = { 0 };
	ret = fs_allocate_blocks(disk, indirect_data.size / BLOCK_SIZE, &indirect_data_addresses);
	if (ret != SUCCESS) return ret;
	
	// Store a single indirect in the inode (i.e. 64 Block Sequences)
	if (indirect_data_addresses->num_elements == 0) {
		/** DONE **/
	}

	BlockSequence* indode_indirect = indirect_data_addresses->head->element;
	inode->data.indirect = *indode_indirect;

	LList* remaining_indirects = llist_create_sublist(*indirect_data_addresses, 1, &ret);
	if (ret != SUCCESS) return ret;

	if (remaining_indirects->num_elements == 0) {
		/** DONE **/
	}

	// Write to double indirect data
	HeapData double_indirect_data = { 0 };
	mem_alloc(&double_indirect_data, remaining_indirects->num_elements * BLOCK_SEQ_SIZE);
	
	current = remaining_indirects->head;
	for (int i = 0; i < remaining_indirects->num_elements; i++) {
		BlockSequence* seq = current->element;
		_stream_write_seq_to_heap(*seq, &double_indirect_data, i * BLOCK_SEQ_SIZE);
		current = current->next;
	}

	LList* double_indirect_addresses;
	ret = fs_allocate_blocks(disk, double_indirect_data.size / BLOCK_SIZE, &double_indirect_addresses);
	if (ret != SUCCESS) return ret;

	// Write an d-indirect to the inode
	if (double_indirect_addresses->num_elements > 0) {
		BlockSequence* inode_double_indirect = double_indirect_addresses->head->element;
		inode->data.double_indirect = *inode_double_indirect;
	}

	LList* remaining_double_indirects = llist_create_sublist(*double_indirect_addresses, 1, &ret);
	if (ret != SUCCESS) return ret;

	if (remaining_double_indirects->num_elements == 0) {
		/** DONE **/
	}

	HeapData triple_indirect_data = { 0 };
	mem_alloc(&triple_indirect_data, remaining_double_indirects->num_elements * BLOCK_SEQ_SIZE);

	current = remaining_double_indirects->head;
	for (int i = 0; i < remaining_double_indirects->num_elements; i++) {
		BlockSequence* seq = current->element;
		_stream_write_seq_to_heap(*seq, &triple_indirect_data, i * BLOCK_SEQ_SIZE);
		current = current->next;
	}

	LList* triple_indirect_addresses;
	ret = fs_allocate_blocks(disk, triple_indirect_data.size / BLOCK_SIZE, &triple_indirect_addresses);
	if (ret != SUCCESS) return ret;

	// Write an d-indirect to the inode
	if (triple_indirect_addresses->num_elements > 0) {
		BlockSequence* inode_triple_indirect = triple_indirect_addresses->head->element;
		inode->data.triple_indirect = *inode_triple_indirect;
	}

	return SUCCESS;
}
