#ifndef STREAM_H
#define STREAM_H

#include <stdint.h>
#include "memory.h"
#include "constants.h"
#include "fs.h"
#include "disk.h"

int stream_ds_to_data(DataStream, Disk, HeapData*);
int stream_add_seq_to_data(HeapData* data, BlockSequence seq); 
int stream_write_addresses(Disk* disk, Inode* inode, LList addresses);
int stream_write_addresses_to_heap(LList addresses, HeapData* data);
#endif
