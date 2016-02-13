#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>
#include "memory.h"
#include "fs.h"

typedef HeapData Bitmap; 

int bitmap_write(Bitmap*, int, int);
int bitmap_read(Bitmap, int, int*);
int bitmap_find_continuous_block_run(Bitmap, int, int, int*);
int bitmap_find_block(Bitmap, int, int*);
int bitmap_find_continuous_run_length(Bitmap, int, int*);
#endif 
