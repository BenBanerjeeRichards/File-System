#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "fs.h"
#include "bitmap.h"


int bitmap_write(Bitmap* bitmap, int bit_address, int value){
	// This works due to integer division
	int byte_addr = bit_address / 8;
	int bit = bit_address - (byte_addr * 8);
	int ret = 0;

	uint8_t byte_val = mem_read(*bitmap, byte_addr, &ret);
	if (ret != SUCCESS) return ret;

	if (value) {
		byte_val |= 1 << (7 - bit);
	} else {
		byte_val &= ~(1 << (7 - bit));
	}

	return mem_write(bitmap, byte_addr, byte_val);
}

int bitmap_read(Bitmap bitmap, int bit_address, int* error)
{
	int byte_addr = bit_address / 8;
	int bit = bit_address - (byte_addr * 8);
	int ret = 0;

	uint8_t byte = mem_read(bitmap, byte_addr, &ret);
	if (ret != SUCCESS) return ret;

	return (byte >> (7-bit)) & 1;
}

int bitmap_write_range(Bitmap bitmap, int bit_address_start, int length, int value) {
	int ret = 0;

	for (int i = 0; i < length; i++) {
		ret = bitmap_write(&bitmap, bit_address_start + i, value);
		if (ret != SUCCESS) return ret;
	}

	return SUCCESS;
}

int bitmap_find_continuous_block_run(Bitmap bitmap, int length, int start_byte, int* run_start_bit) {
	if (!bitmap.valid) return ERR_INVALID_BITMAP;	
	int current_bit_count = 0;
	
	for (int i = 0; i < bitmap.size; i++)
	{
		int byte_index = 0;
		if(i + start_byte + 1 > bitmap.size) {
			byte_index = (i + start_byte) - bitmap.size;
		} else {
			byte_index = i + start_byte;
		}
		
		int error = 0;
		for (int j = 0; j < 8; j++) {
			int bit = bitmap_read(bitmap, byte_index * 8 + j, &error);

			if (error != SUCCESS) return error;

			if (bit == 0) {
				current_bit_count += 1;
			} else{
				current_bit_count = 0;
			}
			
			if (current_bit_count == length) {
				*run_start_bit = byte_index * 8 + j + 1 - current_bit_count;
				return SUCCESS;
			}

		}
	}
	
	return ERR_NO_BITMAP_RUN_FOUND;
}

int bitmap_find_block(Bitmap bitmap, int start_byte, int* block_addr) {
	if (!bitmap.valid) return ERR_INVALID_BITMAP;	
	int error = 0;

	for (int i = 0; i < bitmap.size; i++){
		int byte_index = 0;
		if(i + start_byte + 1 > bitmap.size) {
			byte_index = (i + start_byte) - bitmap.size;
		} else {
			byte_index = i + start_byte;
		}

		for (int j = 0; j < 8; j++) {
			int bit = bitmap_read(bitmap, byte_index * 8 + j, &error);

			if (error != SUCCESS) return error;

			if (bit == 0) {
				*block_addr = byte_index * 8 + j;
				return SUCCESS;
			}

		}
	}
	return ERR_BITMAP_NO_FREE_BLOCK;
}	


int bitmap_find_continuous_run_length(Bitmap bitmap, int start_bit, int* run_length) {
	if (!bitmap.valid) return ERR_INVALID_BITMAP;
	if (start_bit < 0 || start_bit > bitmap.size * 8) return ERR_INVALID_MEMORY_ACCESS;
	int error = 0;
	*run_length = 0;

	for (int i = start_bit; i < bitmap.size * 8; i++) {
		int bit = bitmap_read(bitmap, i, &error);
		
		if (bit == 1) {
			// If first bit is 1 then invalid input
			*run_length = i - start_bit;
			if (i == start_bit) return ERR_NO_BITMAP_RUN_FOUND;
			break;
		}
	}
	
	if (*run_length == 0) {
		*run_length = bitmap.size * 8 - start_bit;
	}

	return SUCCESS;
}
