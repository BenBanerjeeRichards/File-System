#ifndef FS_API_H
#define FS_API_H


#include "fs.h"
#include "directory.h"
#include "stream.h"


int api_mount_filesystem(Disk* disk);
int api_unmount_filesystem(Disk disk);
int api_remove_filesystem(Disk disk);

int api_create_file(Disk disk, Permissions permissons, HeapData path);
int api_write_to_file(Disk disk, HeapData data);
int api_read_from_file(Disk disk, int location, int size, HeapData* data);
#endif