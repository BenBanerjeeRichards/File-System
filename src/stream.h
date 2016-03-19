#ifndef STREAM_H
#define STREAM_H

#include <stdint.h>
#include "memory.h"
#include "constants.h"
#include "fs.h"
#include "disk.h"

int stream_write_addresses(Disk* disk, Inode* inode, LList addresses);
int stream_clear_bitmap(Disk* disk, LList* addresses);
int stream_append_to_addresses(Disk disk, Inode* inode, LList new_addresses);
LList* stream_read_addresses(Disk disk, Inode inode, int* error);
LList* stream_read_address_block(Disk disk, BlockSequence block, int* error);
LList* stream_read_alloc_idts(Disk disk, Inode, int* error);

#endif
