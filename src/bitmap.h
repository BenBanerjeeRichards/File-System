#ifndef BITMAP_H
#define BITMAP_H

#include "memory.h"

typedef HeapData Bitmap; 

int bitmap_write(Bitmap* bitmap, int bit_address, int value);
int bitmap_read(Bitmap bitmap, int bit_address, int* value);
int bitmap_find_continuous_block_run(Bitmap, int, int, int*);
int bitmap_find_block(Bitmap, int, int*);
int bitmap_find_continuous_run_length(Bitmap, int, int*);
int bitmap_write_range(Bitmap bitmap, int bit_address_start, int length, int value);
#endif 
