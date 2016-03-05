#ifndef FS_DISK
#define FS_DISK

#include <stdint.h>
#include "memory.h"
#include "constants.h"
#include "fs.h"
#include "bitmap.h"


int disk_mount(Disk*);
int disk_unmount(Disk);
int disk_write(Disk*, int, HeapData);
HeapData disk_read(Disk disk, int location, int size, int* error);

#endif
