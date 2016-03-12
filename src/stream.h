#ifndef STREAM_H
#define STREAM_H

#include <stdint.h>
#include "memory.h"
#include "constants.h"
#include "fs.h"
#include "disk.h"

int stream_write_addresses(Disk* disk, Inode* inode, LList addresses);
LList stream_read_addresses(Disk disk, Inode inode, int* error);
LList* stream_read_address_block(Disk disk, BlockSequence block, int* error);


#endif
