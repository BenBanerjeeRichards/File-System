#ifndef FS_DISK
#define FS_DISK

#include <stdint.h>
#include "memory.h"
#include "constants.h"
#include "fs.h"
#include "bitmap.h"

typedef struct {
	// Lock the entire disk when a process is accessing it
	int is_locked;

	// Kept in memory for performance
	Superblock superblock;
	Bitmap indode_bitmap;

	// Using an in memory disk
	HeapData data;
} Disk;

int disk_mount(Disk*);
int disk_unmount(Disk);
int disk_write(Disk*, int, uint8_t);
uint8_t disk_read(Disk, int, int*);
int disk_write_section(Disk*, int, HeapData);

#endif
