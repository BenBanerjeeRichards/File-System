#ifndef FS_DISK
#define FS_DISK

#include <stdint.h>
#include "memory.h"
#include "constants.h"
#include "fs.h"
#include "bitmap.h"


int disk_mount(Disk*);
int disk_unmount(Disk);
int disk_write(Disk*, int, uint8_t);
uint8_t disk_read(Disk, int, int*);
int disk_write_section(Disk*, int, HeapData);

#endif
