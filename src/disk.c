#include <stdlib.h>
#include <stdio.h>

#include "disk.h"
#include "constants.h"
#include "memory.h"

/*
int check_disk_access(disk_t* disk, int location)
{
	if (!disk->mounted) return ERR_DISK_NOT_MOUNTED;
	if (location < 0 || location > disk->size) return ERR_INVALID_MEMORY_ACCESS;

	return SUCCESS;
} */

int disk_mount(disk_t** disk)
{
	int ret = mem_alloc(*disk, DISK_SIZE);

	return SUCCESS;
}

/*
int disk_unmount(disk_t* disk){
	if (!disk->mounted) return ERR_DISK_NOT_MOUNTED;
	
	disk->mounted = 0;
	free(disk->data);

	return SUCCESS;
}

int disk_read(disk_t* disk, int location, uint8_t* data){
	int ret = check_disk_access(disk, location);
	if (ret != SUCCESS) return ret;

	*data = (disk->data[location]);
	return SUCCESS;
}

int disk_write(disk_t* disk, int location, uint8_t data){
	int ret = check_disk_access(disk, location);
	if (ret != SUCCESS) return ret;

	disk->data[location] = data;
	return SUCCESS;
}
*/