#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "constants.h"

void append_block_sequence_lists(LList* parent, LList sublist) {
	LListNode* current = sublist.head;
	for(int i = 0; i < sublist.num_elements; i++) {
		BlockSequence* seq = current->element;
		BlockSequence* new_seq = malloc(sizeof(BlockSequence));
		memcpy(new_seq, seq, sizeof(BlockSequence));
		llist_insert(parent, new_seq);

		current = current->next;
	} 
}

bool compare_block_sequence(void* el_1, void* el_2) {
	BlockSequence* b1 = el_1;
	BlockSequence* b2 = el_2;
	return (b1->start_addr == b2->start_addr && b1->length == b2->length);
}

void util_print_block_seq_list(LList list) {
	LListNode* current = list.head;
	for (int i = 0; i < list.num_elements; i++) {
		BlockSequence* seq = current->element;
		printf("%i:%i\n", seq->start_addr, seq->length);
		current = current->next;
	}
}

bool block_seq_is_empty(BlockSequence seq) {
	return seq.length == 0 && seq.start_addr == 0;
}

int div_round_up(int a, int b) {
	int quot = a / b;
	if (quot * b == a) {
		return quot;
	}
	return quot + 1;
}

int util_string_to_heap(char* string, HeapData* heap) {
	if (string == NULL) return ERR_NULL_STRING;

	int len = strlen(string);
	if (!len) return ERR_EMPTY_STRING;

	int ret = mem_alloc(heap, len);
	if (ret != SUCCESS) return ret;

	for (int i = 0; i < len; i++) {
		ret = mem_write(heap, i, (uint8_t)string[i]);
		if (ret != SUCCESS) return ret;
	}

	return SUCCESS;
}

uint32_t round_up_nearest_multiple(uint32_t value, uint32_t multiple)
{
	return (uint32_t) multiple * (ceil((double)value/ (double)multiple));
}

int compare_superblock(Superblock s1, Superblock s2){
	int comparison = 0;		// Increment for each difference

	comparison += (s1.magic_1 == s2.magic_1);
	comparison += (s1.version == s2.version);
	comparison += (s1.block_size == s2.block_size);
	comparison += (s1.num_blocks == s2.num_blocks);
	comparison += (s1.num_used_blocks == s2.num_used_blocks);
	comparison += (s1.num_data_blocks == s2.num_data_blocks);
	comparison += (s1.inode_size == s2.inode_size);
	comparison += (s1.num_inodes == s2.num_inodes);
	comparison += (s1.num_used_inodes == s2.num_used_inodes);
	comparison += (s1.inode_bitmap_size == s2.inode_bitmap_size);
	comparison += (s1.inode_table_size == s2.inode_table_size);
	comparison += (s1.data_block_bitmap_size == s2.data_block_bitmap_size);
	comparison += (s1.inode_table_start_addr == s2.inode_table_start_addr);
	comparison += (s1.data_block_bitmap_addr == s2.data_block_bitmap_addr);
	comparison += (s1.data_blocks_start_addr == s2.data_blocks_start_addr);
	comparison += (s1.data_bitmap_circular_loc == s2.data_bitmap_circular_loc);
	comparison += (s1.flags == s2.flags);
	comparison += (s1.magic_1 == s2.magic_1);

	return abs(comparison - 18);
}

int compare_inode(Inode i1, Inode i2) {
	int comparison = 0;
	/*
	comparison +=  (i1.magic == i2.magic);
	comparison +=  (i1.inode_number == i2.inode_number);
	comparison +=  (i1.uid == i2.uid);
	comparison +=  (i1.gid == i2.gid);
	comparison +=  (i1.flags == i2.flags);
	comparison +=  (i1.time_created == i2.time_created);
	comparison +=  (i1.time_last_modified == i2.time_last_modified);
	comparison +=  (i1.size == i2.size);
	comparison +=  (i1.preallocation_size == i2.preallocation_size);

	comparison +=  (i1.data.indirect == i2.data.indirect);
	comparison +=  (i1.data.double_indirect == i2.data.double_indirect);
	comparison +=  (i1.data.triple_indirect == i2.data.triple_indirect);
	*/
	for (int i = 0; i < DIRECT_BLOCK_NUM; i++){
		comparison += (i1.data.direct[i].start_addr == i2.data.direct[i].start_addr);
		comparison += (i1.data.direct[i].length == i2.data.direct[i].length);
	}
	return abs(comparison - 12 - 2 * DIRECT_BLOCK_NUM);

}

int util_write_uint16(HeapData* data, int location, uint16_t value){
	int ret = mem_write(data, location, value >> 8);
	if (ret != SUCCESS) return ret;

	ret = mem_write(data, location + 1, value & 0xFF);
	if (ret != SUCCESS) return ret;

	return SUCCESS;
}

int util_write_uint32(HeapData* data, int location, uint32_t value){
	int ret = util_write_uint16(data, location, value >> 16);
	if (ret != SUCCESS) return ret;

	ret = util_write_uint16(data, location + 2, value & 0xFFFF);
	if (ret != SUCCESS) return ret;

	return SUCCESS;
}

int util_write_uint64(HeapData* data, int location, uint64_t value){
	int ret = util_write_uint32(data, location, value >> 32);
	if (ret != SUCCESS) return ret;

	ret = util_write_uint32(data, location + 4, value & 0xFFFFFFFF);
	if (ret != SUCCESS) return ret;

	return SUCCESS;
}


uint16_t util_read_uint16(HeapData data, int location, int* function_status){
	int status = 0;

	uint8_t msb = mem_read(data, location, &status);
	if (status != SUCCESS){
		*function_status = status;
		return 0;
	}

	uint8_t lsb = mem_read(data, location + 1, &status);
	if (status != SUCCESS){
		*function_status = status;
		return 0;
	}

	*function_status = SUCCESS;
	return (uint16_t)msb << 8 | lsb;
}

uint32_t util_read_uint32(HeapData data, int location, int* function_status)
{
	int status = 0;

	uint16_t msb = util_read_uint16(data, location, &status);
	if (status != SUCCESS)
	{
		*function_status = status;
		return 0;
	} 

	uint16_t lsb = util_read_uint16(data, location + 2, &status);
	if (status != SUCCESS)
	{
		*function_status = status;
		return 0;
	} 

	return (uint32_t)msb << 16 | lsb;
}

uint64_t util_read_uint64(HeapData data, int location, int* function_status)
{
	int status = 0;

	uint32_t msb = util_read_uint32(data, location, &status);
	if (status != SUCCESS)
	{
		*function_status = status;
		return 0;
	} 

	uint32_t lsb = util_read_uint32(data, location + 4, &status);
	if (status != SUCCESS)
	{
		*function_status = status;
		return 0;
	} 

	return (uint64_t)msb << 32 | lsb;
}


void free_element_standard(void* element) {
	free(element);
}

void free_element_bl_debug(void* element) {
	BlockSequence* seq = element;
	free(seq);
}