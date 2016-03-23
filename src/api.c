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

int api_write_to_file(Disk disk, HeapData path, HeapData data) {
	int ret = 0;
	Inode inode = fs_get_inode_from_path(disk, path, &ret);
	if(ret != SUCCESS) return ret;

	inode.size += data.size;

	HeapData remaining = {0};
	ret = fs_fill_unused_allocated_data(&disk, &inode, data, &remaining);
	if(ret != SUCCESS) return ret;

	// allocate more blocks for the remaining data
	if(remaining.size == 0) return SUCCESS;

	LList* addresses = {0};
	ret = fs_allocate_blocks(&disk, div_round_up(remaining.size, BLOCK_SIZE), &addresses);
	if(ret != SUCCESS) return ret;

	if(inode.size == 0) {
		ret = stream_append_to_addresses(disk, &inode, *addresses);
		if(ret != SUCCESS) return ret;
	} else {
		ret = stream_write_addresses(&disk, &inode, *addresses);
		if(ret != SUCCESS) return ret;
	}

	// Finally write the data to the disk
	ret = fs_write_data_to_disk(&disk, remaining, *addresses, true);
	if(ret != SUCCESS) return ret;

	ret = fs_write_inode_data(disk, inode, inode.inode_number);
	if(ret != SUCCESS) return ret;

	mem_free(remaining);

	return SUCCESS;
}

int api_read_all_from_file(Disk disk, int inode_number, HeapData* read_data) {
	int ret = 0;

	Inode inode = fs_read_inode(disk, inode_number, &ret);
	if(ret != SUCCESS) return ret;

	LList* addresses = stream_read_addresses(disk, inode, &ret);
	if(ret != SUCCESS) return ret;

	HeapData read = fs_read_from_disk(disk, *addresses, true, &ret);
	if(ret != SUCCESS) return ret;

	// Trim to actual data
	if(read.size < inode.size) {
		return ERR_INSUFFICIENT_DATA_READ;
	}

	HeapData read_trimmed = {0};
	ret = mem_alloc(&read_trimmed, inode.size);
	if(ret != SUCCESS) return ret;

	memcpy(read_trimmed.data, read.data, inode.size);
	*read_data = read_trimmed;
	mem_free(read);

	return SUCCESS;
}

// TODO complete this function
int api_read_from_file(Disk disk, int inode_number, uint64_t start_read_byte, uint64_t read_length_bytes, HeapData* read_data) {
	int ret = 0;

	Inode inode = fs_read_inode(disk, inode_number, &ret);
	if(ret != SUCCESS) return ret;
	
	LList* addresses = stream_read_addresses(disk, inode, &ret);
	if(ret != SUCCESS) return ret;

	LListNode* current = addresses->head;
	LListNode* prev = NULL;

	LList* read_addresses = llist_new();
	read_addresses->free_element = &free_element_standard;

	uint64_t previous_bytes = 0;
	uint64_t bytes_read = 0;

	bool start_reading = false;

	for(int i = 0; i < addresses->num_elements; i++) {
		BlockSequence* seq = current->element;

		if(previous_bytes >= start_read_byte && bytes_read < read_length_bytes) {
			llist_insert(read_addresses, seq);
			bytes_read += seq->length * BLOCK_SIZE;
		} else {
			previous_bytes += seq->length * BLOCK_SIZE;
		}

		if(read_length_bytes > bytes_read) break;

		// TODO update prev
		current = current->next;
	}

	HeapData read = fs_read_from_disk(disk, *read_addresses, true, &ret);
	HeapData read_final = {0};
	mem_alloc(&read_final, read_length_bytes);


	llist_free(read_addresses);
	llist_free(addresses);
	return SUCCESS;
}
