#include <stdlib.h>
#include <stdio.h>

#include "disk.h"
#include "constants.h"
#include "memory.h"

int disk_mount(Disk* disk) {
	return mem_alloc(&disk->data, DISK_SIZE);
}

int disk_unmount(Disk disk) {
	return mem_free(disk.data);
}

int disk_write(Disk* disk, int location, uint8_t data) {
	return mem_write(&disk->data, location, data);
}

uint8_t disk_read(Disk disk, int location, int* error) {
	return mem_read(disk.data, location, error);
}

int disk_write_section(Disk* disk, int location, HeapData data) {
	return mem_write_section(&disk->data, location, data);
}
