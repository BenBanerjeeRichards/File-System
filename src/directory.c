#include "directory.h"

int dir_add_entry(Directory* directory, DirectoryEntry entry) {
	if (entry.name.size > 0xFF) return ERR_INODE_NAME_TOO_LARGE;
	const int entry_size = entry.name.size + 5;
	int ret = 0;

	if (!directory->valid) {
		ret = mem_alloc(directory, entry_size);
	}
	else {
		ret = mem_realloc(directory, entry_size + directory->size);
	}
	if (ret != SUCCESS) return ret;

	const int INCREMENT_8 = 1;
	const int INCREMENT_16 = 2;
	const int INCREMENT_32 = 4;

	int current_location = directory->size - entry_size;

	ret = util_write_uint32(directory, current_location, entry.inode_number);
	if (ret != SUCCESS) return ret;
	current_location += INCREMENT_32;

	ret = mem_write(directory, current_location, entry.name.size);
	if (ret != SUCCESS) return ret;
	current_location += INCREMENT_8;

	ret = mem_write_section(directory, current_location, entry.name);
	if (ret != SUCCESS) return ret;

	return SUCCESS;
}

int dir_get_inode_number(Directory directory, HeapData name, uint32_t* inode_number) {
	if (!directory.valid) return ERR_INVALID_MEMORY_ACCESS;
	if (!name.valid) return ERR_INVALID_MEMORY_ACCESS;

	// Always referes to the *start* of each directory entry
	int current_location = 0;

	while (current_location < directory.size) {
		int name_start = current_location + 5;
		int error = 0;
		if (name_start + name.size > directory.size) return ERR_INODE_NOT_FOUND;

		uint32_t inode_num = util_read_uint32(directory, current_location, &error);
		if (error != SUCCESS) return error;

		uint8_t name_size = mem_read(directory, current_location + 4, &error);
		if (error != SUCCESS) return error;

		if (name_size != name.size) {
			current_location += 5 + name_size;
			continue;
		}

		if (memcmp(&directory.data[current_location + 5], name.data, name.size) == 0) {
			*inode_number = inode_num;
			return SUCCESS;
		}

		current_location += 5 + name_size;
	}

	return ERR_INODE_NOT_FOUND;
}

int dir_find_next_path_name(HeapData path, int start, HeapData* name) {
	if (!path.valid) return ERR_INVALID_MEMORY_ACCESS;
	if (start < 0 || start > path.size) return ERR_INVALID_MEMORY_ACCESS;
	int error = 0;
	int current_name_len = 0;

	for (int i = start; i < path.size; i++) {
		uint8_t byte = mem_read(path, i, &error);
		if (error != SUCCESS) return error;

		// Check for forward slash
		if (byte == ASCII_FORWARD_SLASH) {
			break;
		}

		current_name_len += 1;
		// If code exits loop normally, then it is the end of the string
		// which terminates the current dir name (in addition to a forwards 
		// slash)
	}
	int ret = mem_alloc(name, current_name_len);
	if (ret != SUCCESS) return ret;

	memcpy(name->data, &path.data[start], current_name_len);
	name->size = current_name_len;
	return SUCCESS;
}

int dir_get_directory(Disk disk, HeapData path, Directory start, DirectoryEntry* directory) {
	bool found_entry = false;
	int ret = 0;
	Directory current = start;
	DirectoryEntry previous_entry = {0};
	int current_path_position = 0;
	unsigned int inode_num = 0;

	while(true) {
		HeapData name = {0};
		ret = dir_find_next_path_name(path, current_path_position, &name);
		if(ret == ERR_INVALID_MEMORY_ACCESS) {
			*directory = previous_entry;
			return SUCCESS;
		}

		if(ret != SUCCESS && ret != ERR_INVALID_MEMORY_ACCESS) return ret;
		current_path_position += name.size + 1;
		
		ret = dir_get_inode_number(current, name, &inode_num);
		if(ret != SUCCESS) return ret;
		
		previous_entry.inode_number = inode_num;
		previous_entry.name = name;

		// Read inode
		const double block_addr = inode_addr_to_disk_block_addr(disk, inode_num);
		current = disk_read(disk, block_addr * BLOCK_SIZE, INODE_SIZE, &ret);
		if(ret != SUCCESS) return ret;

		Inode inode;
		ret = unserialize_inode(&current, &inode);
		if(ret != SUCCESS) return ret;

		// Read directory as pointed to by inode
		// This could be put into its own function
		LList* dir_addresses = stream_read_addresses(disk, inode, &ret);
		if(ret != SUCCESS) return ret;

		current = fs_read_from_disk(disk, *dir_addresses, true, &ret);
		if(ret != SUCCESS) return ret;
	}

	return SUCCESS;
}

int dir_get_path_name(HeapData path, HeapData* name) {
	for(int i = path.size - 1; i >= 0; i--) {
		if(path.data[i] == ASCII_FORWARD_SLASH) {
			HeapData file_name = {0};
			mem_alloc(&file_name, path.size - i - 1);
			memcpy(file_name.data, &path.data[i + 1], file_name.size);
			*name = file_name;
			return SUCCESS;
		}
	}

	return IS_ROOT_FILE;

}

int dir_add_to_directory(Disk disk, HeapData path, DirectoryEntry entry) {
	// DirectoryEntry represents a file which has been allocated and stored on disk
	// but not placed in a directory.
	int ret = 0;	

	// Get root directory
	Inode inode = fs_read_inode(disk, ROOT_DIRECTORY_INODE_NUMBER, &ret);
	if(ret != SUCCESS) return ret;

	LList* root_addresses = stream_read_addresses(disk, inode, &ret);
	if(ret != SUCCESS) return ret;

	HeapData root_data = fs_read_from_disk(disk, *root_addresses, true, &ret);
	if(ret != SUCCESS) return ret;

	// Find directory
	DirectoryEntry parent_entry = {0};

	if(path.size == 0) {
		// Write at root
		parent_entry.inode_number = ROOT_DIRECTORY_INODE_NUMBER;
	} else {
		ret = dir_get_directory(disk, path, root_data, &parent_entry);
		if(ret != SUCCESS) return ret;
	}

	Directory directory = {0};
	ret = dir_add_entry(&directory, entry);
	if(ret != SUCCESS) return ret;

	ret = fs_write_to_file(&disk, parent_entry.inode_number, directory);
	if(ret != SUCCESS) return ret;

	return SUCCESS;
}