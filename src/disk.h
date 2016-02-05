#ifndef FS_DISK
#define FS_DISK

#include <stdint.h>
#include "memory.h"

#define DISK_SIZE MEGA

/*
typedef struct {
	uint8_t* data;
	int size;
	int mounted;
} disk_t;
*/

typedef HeapData disk_t;

int check_disk_access(disk_t*, int);

int disk_mount(disk_t**);
int disk_unmount(disk_t*);

int disk_read(disk_t*, int, uint8_t*);
int disk_write(disk_t*, int, uint8_t);

#endif