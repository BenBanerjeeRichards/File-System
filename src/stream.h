#ifndef STREAM_H
#define STREAM_H

#include <stdint.h>
#include "memory.h"
#include "constants.h"
#include "fs.h"
#include "disk.h"

int stream_ds_to_data(DataStream, Disk, HeapData*);
int stream_add_seq_to_data(HeapData* data, BlockSequence seq); 

#endif
