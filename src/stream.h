#ifndef STREAM_H
#define STREAM_H

#include <stdint.h>
#include "memory.h"
#include "constants.h"
#include "fs.h"
#include "disk.h"

int stream_write_addresses(Disk* disk, Inode* inode, LList addresses);
int stream_write_addresses_to_heap(LList addresses, HeapData* data);
#endif
