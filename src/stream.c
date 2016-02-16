#include "stream.h"
#include "util.h"
#include <string.h>

int stream_add_seq_to_data(HeapData* data, BlockSequence seq) {
	int ret = 0;
	if(!data->valid) {
		ret = mem_alloc(data, 2);	
	} else {
		mem_realloc(data, data->size + 2);
	}
	if (ret != SUCCESS) return ret;

	ret = util_write_uint32(data, data->size - 2, seq.start_addr);
	if (ret != SUCCESS) return ret;

	ret = util_write_uint32(data, data->size - 1, seq.length);
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
