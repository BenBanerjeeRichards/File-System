#include "api.h"

int api_mount_filesystem(Disk* disk) {
	int ret = 0;

	*disk = fs_create_filesystem(FILESYSTEM_FILE_NAME, DISK_SIZE, &ret);
	if(ret != SUCCESS) return ret;

	// Allocate space for root inode
	ret = bitmap_write(&disk->inode_bitmap, ROOT_DIRECTORY_INODE_NUMBER, 1);
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

int api_create_file(Disk disk, Permissions permissons, HeapData path) {
	Inode inode = {0};
	inode.uid = permissons.uid;
	inode.gid = permissons.gid;
	inode.magic = INODE_MAGIC;

	int inode_num = 0;
	int ret = fs_write_inode(disk, &inode, &inode_num);
	if(ret != SUCCESS) return ret;

	DirectoryEntry entry = {0};
	HeapData name = {0};
	ret = dir_get_path_name(path, &name);
	if(ret != SUCCESS) return ret;

	entry.name = name;
	entry.inode_number = inode_num;

	const int path_size = path.size;
	path.size -= (name.size + 1);

	ret = dir_add_to_directory(disk, path, entry);
	if(ret != SUCCESS) return ret;

	path.size = path_size;
	return SUCCESS;
}
int api_read_all_from_file(Disk disk, int inode_number, HeapData* read_data) {
	int ret = 0;

	Inode inode = fs_read_inode(disk, inode_number, &ret);
	if(ret != SUCCESS) return ret;

	LList* addresses = stream_read_addresses(disk, inode, &ret);
	if(ret != SUCCESS) return ret;

	*read_data = fs_read_from_disk(disk, *addresses, true, &ret);
	if(ret != SUCCESS) return ret;

	return SUCCESS;
}
