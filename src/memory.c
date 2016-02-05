#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "memory.h"
#include "constants.h"

int mem_check_access(HeapData* heap, int location)
{
	if (!heap->valid) return ERR_INVALID_MEMORY_ACCESS;
	if (location < 0 || location > heap->size) return ERR_INVALID_MEMORY_ACCESS;

	return SUCCESS;
}


int mem_alloc(HeapData* heap, int size){
	heap->valid = 0;

	// malloc(0) is not well defined - inconsistant depending on implementation
	if (size <= 0) return ERR_INVALID_MEM_ALLOC;

	size = size * sizeof(uint8_t);
	heap->data = malloc(size);

	if(heap->data == NULL) {
		malloc_failed();
		return ERR_MALLOC_FAILED;
	}

	memset(heap->data, 0, size);

	heap->size = size;
	heap->valid = 1;
	return SUCCESS;
}

int mem_realloc(HeapData* heap, int size) {
	if (!heap->valid) return ERR_INVALID_MEMORY_ACCESS;
	if (size <= 0) return ERR_INVALID_MEM_ALLOC;

	heap->data = realloc(heap->data, size);
	if (heap->data == NULL) return ERR_MALLOC_FAILED;
	heap->size = size;
	return SUCCESS;
}

int mem_free(HeapData heap){
	if (!heap.valid) return ERR_INVALID_MEMORY_ACCESS;
	heap.valid = 0;
	free(heap.data);
	return SUCCESS;
}

int mem_write(HeapData* heap, int location, uint8_t data)
{
	int ret = mem_check_access(heap, location);
	if (ret != SUCCESS) return ret;

	heap->data[location] = data;
	return SUCCESS;
}

int mem_write_section(HeapData* heap, int location, HeapData* data) {
	int ret = mem_check_access(heap, location);
	if (ret != SUCCESS) return ret;
	if (!data->valid || data->size <=0) return ERR_INVALID_MEMORY_ACCESS;
	if (data->size + location > heap->size) return ERR_INVALID_MEMORY_ACCESS;

	memcpy(&heap->data[location], data->data, data->size * sizeof(uint8_t));
	return SUCCESS;
}

uint8_t mem_read(HeapData* heap, int location, int* function_status){
	int ret = mem_check_access(heap, location);
	if (ret != SUCCESS){
		*function_status = ret;
		return 0;
	}

	return heap->data[location];
}

int mem_write_binary(void* stream, int length, char* path) {
	if(path == NULL) return ERR_INVALID_FILE_PATH;

	FILE* fp = fopen(path, "wb");
	if (fp == NULL) return ERR_FILE_ACCESS_FAILED;

	size_t written = fwrite(stream, sizeof(uint8_t), length, fp);
	fclose(fp);

	if (written != length) return ERR_PARTIAL_FILE_WRITE;
	return SUCCESS;

}

int mem_dump(HeapData* heap, char* path){
	if (!heap->valid) return ERR_INVALID_MEMORY_ACCESS;
	return mem_write_binary(heap->data, heap->size, path);
}	

int mem_dump_section(HeapData* heap, char* path, int start_location, int length) {
	int ret = mem_check_access(heap, start_location);
	if (ret != SUCCESS) return ret;
	if (start_location + length > heap->size) return ERR_INVALID_MEMORY_ACCESS;

	return mem_write_binary(&heap->data[start_location], length, path);
}


void malloc_failed(){
	// Emergency damage control
}