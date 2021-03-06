#ifndef FS_API_H
#define FS_API_H


#include "fs.h"

int api_mount_filesystem(Disk* disk);
int api_unmount_filesystem(Disk disk);
int api_remove_filesystem(Disk disk);

int api_create_file(Disk disk, Permissions permissons, HeapData path);
int api_write_to_file(Disk disk, HeapData path, HeapData data);
int api_read_all_from_file(Disk disk, int inode_number, HeapData* read_data);
int api_delete_file(Disk* disk, HeapData path);
int api_create_dir(Disk* disk, HeapData path, HeapData directory_name);
int api_list_directory(Disk disk, HeapData path, LList** items);
#endif