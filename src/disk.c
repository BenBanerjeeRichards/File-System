#include <stdlib.h>
#include <stdio.h>

#include "disk.h"
#include "constants.h"
#include "bitmap.h"
#include "memory.h"

int disk_mount(Disk* disk) {
	// During testing
	remove(FILESYSTEM_FILE_NAME);

	// ab+ - open for reading and writing, create file if needed
	disk->file = fopen(FILESYSTEM_FILE_NAME, "wb+");

	HeapData data = { 0 };
	// This also memsets all data to 0
	mem_alloc(&data, DISK_SIZE);

	return disk_write(disk, 0, data);
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
	if (data.size > DISK_SIZE - location) return ERR_DATA_TOO_LARGE;

	int ret = fseek(disk->file, location, SEEK_SET);
	if (ret == -1) return ERR_INVALID_FILE_OPERATION;

	ret = fwrite(data.data, sizeof(uint8_t), data.size, disk->file);
	if (ret == -1) return ERR_INVALID_FILE_OPERATION;


	printf("Wrote to %i size %i\n", location, data.size);
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
	if (size + location > DISK_SIZE) {
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
