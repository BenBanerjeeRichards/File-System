#include <stdlib.h>
#include <stdio.h>

#include "disk.h"
#include "constants.h"
#include "memory.h"

int disk_mount(Disk* disk) {
	return mem_alloc(disk, DISK_SIZE);
}

int disk_unmount(Disk disk) {
	return mem_free(disk);
}

int disk_write(Disk* disk, int location, uint8_t data) {
	return mem_write(disk, location, data);
}

uint8_t disk_read(Disk disk, int location, int* error) {
	return mem_read(disk, location, error);
}

int disk_write_section(Disk* disk, int location, HeapData data) {
	return mem_write_section(disk, location, data);
}
