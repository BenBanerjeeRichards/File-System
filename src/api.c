#include "api.h"

int api_mount_filesystem(Disk* disk) {
	int ret = 0;

	*disk = fs_create_filesystem(FILESYSTEM_FILE_NAME, DISK_SIZE, &ret);
	if(ret != SUCCESS) return ret;

	return SUCCESS;
}

int api_unmount_filesystem(Disk disk) {
	int ret = disk_unmount(disk);
	if(ret != SUCCESS) return ret;

	ret = disk_free(disk);
	if(ret != SUCCESS) return ret;

	return SUCCESS;
}

int api_remove_filesystem(Disk disk) {
	return disk_remove(disk.filename);
}