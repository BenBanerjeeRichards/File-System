#ifndef FS_DIRECTORY_H
#define FS_DIRECTORY_H

#include "fs.h"
#include "constants.h"
#include "util.h"


int dir_add_entry(Directory* directory, DirectoryEntry entry);
int dir_get_inode_number(Directory directory, HeapData name, uint32_t* inode_number);
int dir_find_next_path_name(HeapData path, int start, HeapData* name);
#endif