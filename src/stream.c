#include "stream.h"
#include "util.h"
#include <string.h>
#include <math.h>

int stream_add_seq_to_data(HeapData* data, BlockSequence seq) {
	int ret = 0;
	if(!data->valid) {
		ret = mem_alloc(data, 8);	
	} else {
		mem_realloc(data, data->size + 2);
	}
	if (ret != SUCCESS) return ret;

	ret = util_write_uint32(data, data->size - 8, seq.start_addr);
	if (ret != SUCCESS) return ret;

	ret = util_write_uint32(data, data->size - 4, seq.length);
	if (ret != SUCCESS) return ret;

	return SUCCESS;
}

int stream_add_indirect_to_data(HeapData* data, HeapData indirect) {	
	if (!indirect.valid) return ERR_INVALID_MEMORY_ACCESS;
	int ret = 0;
	
	if (!data->valid) {
		ret = mem_alloc(data, indirect.size);
	} else {
		ret = mem_realloc(data, data->size + indirect.size);
	}
	if (ret != SUCCESS) return ret;

	// Firstly determine how much data needs to be transferred
	int i = 0;
	while (i < indirect.size) {
		uint8_t data = mem_read(indirect, i + 1, &ret);
		if (ret != SUCCESS) return ret;

		if (data == 0) break;
		i += 2;
	}

	memcpy(&data->data[data->size - indirect.size], indirect.data, i);
	
	return SUCCESS;

}

int stream_ds_to_data(DataStream stream, Disk disk, HeapData* data) {	
	int ret = 0;
	for (int i = 0; i < DIRECT_BLOCK_NUM; i++) {
		if (stream.direct[i].length == 0) {
			return SUCCESS;
	}

		ret = stream_add_seq_to_data(data, stream.direct[i]);
		if (ret != SUCCESS) return ret;
	}

	return SUCCESS;
}

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
	// Firstly calculate data that needs to be alloc'd for storing non-directs
	const int num_non_directs = addresses.num_elements < DIRECT_BLOCK_NUM ? 0 : addresses.num_elements - DIRECT_BLOCK_NUM;
	const int addresses_per_block = BLOCK_SIZE / BLOCK_SEQ_SIZE;
	const int num_non_direct_blocks = ceil((double)num_non_directs / (double)addresses_per_block);

	// Allocate non direct blocks
	LList* non_direct_block_addresses;
	int ret = fs_allocate_blocks(disk, num_non_direct_blocks * BLOCK_SIZE, &non_direct_block_addresses);
	if (ret != SUCCESS) return ret;

	HeapData serialized_address = { 0 };
	ret = mem_alloc(&serialized_address, (addresses.num_elements - DIRECT_BLOCK_NUM) * ADDRESS_SIZE);
	if (ret != SUCCESS) return ret;

	LListNode* current = addresses.head;

	for (int i = 0; i < addresses.num_elements; i++) {
		if (i < DIRECT_BLOCK_NUM) {
			inode->data.direct[i] = *(BlockSequence*)current->element;
		}
		else {
			BlockSequence* seq = current->element;
			_stream_write_seq_to_heap(*seq, &serialized_address, (i - DIRECT_BLOCK_NUM) * ADDRESS_SIZE);
		}

		current = current->next;
	}

	if (serialized_address.size > 0) {
		// Write the addresses to the file
		ret = fs_write_data_to_disk(disk, serialized_address, *non_direct_block_addresses);
		mem_free(serialized_address);
		llist_free(non_direct_block_addresses);
		if (ret != SUCCESS) return ret;
	}

	return SUCCESS;

}
