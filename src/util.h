#ifndef FS_UTIL
#define FS_UTIL

#include "fs.h"
#include "memory.h"

uint32_t round_up_nearest_multiple(uint32_t, uint32_t);
int util_string_to_heap(char*, HeapData*);
int util_path_next_dir_name(HeapData, int, HeapData*);

int compare_superblock(Superblock, Superblock);
int compare_inode(Inode, Inode);

int util_write_uint16(HeapData*, int, uint16_t);
int util_write_uint32(HeapData*, int, uint32_t);
int util_write_uint64(HeapData*, int, uint64_t);

uint16_t util_read_uint16(HeapData, int, int*);
uint32_t util_read_uint32(HeapData, int, int*);
uint64_t util_read_uint64(HeapData, int, int*);

// Free callbacks 
void free_element_standard(void* element);
#endif
