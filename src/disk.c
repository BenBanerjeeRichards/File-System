#include <stdlib.h>
#include <stdio.h>

#include "disk.h"
#include "constants.h"
#include "bitmap.h"
#include "memory.h"

#include <time.h>

int disk_mount(Disk* disk) {
	// During testing
	remove(FILESYSTEM_FILE_NAME);

	disk->file = fopen(FILESYSTEM_FILE_NAME, "wb+");
	HeapData data = { 0 };
	// This also memsets all data to 0
	mem_alloc(&data, disk->size);

	int ret = disk_write(disk, 0, data);
	mem_free(data);
	return ret;
}

int disk_unmount(Disk disk) {
	if (disk.file == NULL) return ERR_INVALID_FILE_OPERATION;
	int ret = fclose(disk.file);
	if (ret == EOF) return ERR_FILE_OPERATION_FAILED;
	return SUCCESS;
}

int disk_write(Disk* disk, int location, HeapData data) {
	if (!data.valid) return ERR_INVALID_MEMORY_ACCESS;
	if (disk->file == NULL) return ERR_INVALID_FILE_OPERATION;
	if (location < 0) return ERR_INVALID_FILE_OPERATION;
	if (data.size > disk->size - location) return ERR_DATA_TOO_LARGE;

	int ret = fseek(disk->file, location, SEEK_SET);
	if (ret == -1) return ERR_INVALID_FILE_OPERATION;

	ret = fwrite(data.data, sizeof(uint8_t), data.size, disk->file);
	if (ret == -1) return ERR_INVALID_FILE_OPERATION;
	if (ret != data.size) return ERR_PARTIAL_FILE_WRITE;
	return SUCCESS;
}

HeapData disk_read(Disk disk, int location, int size, int* error) {
	HeapData data_read = { 0 };
	if (!disk.file) {
		*error = ERR_INVALID_FILE_OPERATION;
		return data_read;
	}
	if (location < 0) {
		*error = ERR_INVALID_FILE_OPERATION;
		return data_read;
	}
	if (size < 0) {
		*error = ERR_INVALID_FILE_OPERATION;
		return data_read;
	}
	if (size + location > disk.size) {
		*error = ERR_DATA_TOO_LARGE;
		return data_read;
	}

	int ret = mem_alloc(&data_read, size);
	if (ret != SUCCESS) {
		*error = ret;
		return data_read;
	}

	ret = fseek(disk.file, location, SEEK_SET);
	if (ret == -1) {
		*error = ERR_FILE_OPERATION_FAILED;
		return data_read;
	}


	ret = fread(data_read.data, sizeof(uint8_t), data_read.size, disk.file);
	if (ret != size) {
		*error = ERR_PARTIAL_FILE_READ;
		return data_read;
	}

	*error = SUCCESS;
	return data_read;

}

int disk_write_offset(Disk* disk, int location, int offset, HeapData data) {
	// Writes data at an offset
	// i.e. starts write at location + offset.
	// Will wrap data to start of offset if needed

	if (location < 0 || location > disk->size) return ERR_INVALID_FILE_OPERATION;
	if (offset < 0 || offset > disk->size) return ERR_INVALID_FILE_OPERATION;
	if (!data.valid) return ERR_INVALID_MEMORY_ACCESS;
	if (data.size <= 0) return ERR_INVALID_MEMORY_ACCESS;

	int ret = 0;
	int start_write_loc = offset + location;
	
	if (start_write_loc + data.size > disk->size) {
		// Create data subsets
		// data_1: From start_write_loc to end of the file
		// data_2: From beginning of file to data.size - data_1_size
		int data_1_size = disk->size - start_write_loc;
		int data_2_size = data.size - data_1_size;

		// Write to the end of the file (data 1)
		int temp_size = data.size;
		data.size = data_1_size;
		ret = disk_write(disk, start_write_loc, data);
		data.size = temp_size;
		if (ret != SUCCESS) return ret;

		// Write from beginning of file (data 2)
		HeapData data_2 = { 0 };
		ret = mem_alloc(&data_2, data_2_size);
		if (ret != SUCCESS) return ret;

		memcpy(data_2.data, &data.data[data_1_size], data_2_size);
		ret = disk_write(disk, offset, data_2);
		mem_free(data_2);
		if (ret != SUCCESS) return ret;


	}
	else {
		return disk_write(disk, start_write_loc, data);
	}

	return SUCCESS;

}

HeapData disk_read_offset(Disk disk, int location, int offset, int size, int* error){
	HeapData read_data = { 0 };
	if (location < 0 || location > disk.size) {
		*error = ERR_INVALID_FILE_OPERATION;
		return read_data;
	}

	if (offset < 0 || offset > disk.size) {
		*error = ERR_INVALID_FILE_OPERATION;
		return read_data;
	}

	if (size <= 0) {
		*error = ERR_INVALID_MEMORY_ACCESS;
		return read_data;
	}

	if (size < 0 || size > disk.size) {
		*error = ERR_INVALID_FILE_OPERATION;
		return read_data;
	}

	int ret = 0;
	int start_read_loc = offset + location;

	if (start_read_loc + size > disk.size) {
		int read_1_size = disk.size - start_read_loc;
		int read_2_size = size - read_1_size;

		HeapData read_1 = disk_read(disk, start_read_loc, read_1_size, &ret);
		if (ret != SUCCESS) {
			*error = ret;
			return read_data;
		}

		HeapData read_2 = disk_read(disk, offset, read_2_size, &ret);
		if (ret != SUCCESS) {
			*error = ret;
			return read_data;
		}
	
		// Combine together
		mem_alloc(&read_data, read_1.size + read_2.size);
		memcpy(read_data.data, read_1.data, read_1.size);
		memcpy(&read_data.data[read_1.size], read_2.data, read_2.size);
		mem_free(read_1);
		mem_free(read_2);
		return read_data;
	}
	else {
		read_data = disk_read(disk, offset, size, &ret);
		if (ret != SUCCESS) {
			*error = ret;
		}

		return read_data;
	}
}
int disk_remove(const char* name) {
	int ret = remove(name);
	return (ret == 0) ? SUCCESS : ERR_FILE_OPERATION_FAILED;
}
