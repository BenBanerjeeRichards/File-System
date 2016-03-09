#ifndef FS_DISK
#define FS_DISK

#include <stdint.h>
#include "memory.h"
#include "constants.h"
#include "fs.h"
#include "bitmap.h"

int disk_mount(Disk* disk, const char* name);
int disk_unmount(Disk);
int disk_write(Disk*, int, HeapData);
int disk_write_offset(Disk* disk, int location, int offset, HeapData data);
int disk_remove(const char* name);
HeapData disk_read(Disk disk, int location, int size, int* error);
HeapData disk_read_offset(Disk disk, int location, int offset, int size, int* error);
#endif
