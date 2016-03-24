#ifndef FS_DIRECTORY_H
#define FS_DIRECTORY_H

#include "fs.h"
#include "constants.h"
#include "util.h"
#include "disk.h"
#include "serialize.h"
#include "stream.h"
#include "api.h"

int dir_add_entry(Directory* directory, DirectoryEntry entry);
int dir_get_inode_number(Directory directory, HeapData name, uint32_t* inode_number);
int dir_find_next_path_name(HeapData path, int start, HeapData* name);
int dir_get_directory(Disk disk, HeapData path, Directory start, DirectoryEntry* directory);
int dir_add_to_directory(Disk disk, HeapData path, DirectoryEntry entry);
int dir_get_path_name(HeapData path, HeapData* name);
int dir_remove_entry(Disk* disk, HeapData path, HeapData name, Directory* new_dir);

DirectoryEntry dir_read_next_entry(Directory directory, int start, int* ret);

#endif