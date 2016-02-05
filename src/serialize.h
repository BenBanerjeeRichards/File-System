#ifndef FS_SERIALIZE
#define FS_SERIALIZE

#include <stdint.h>
#include "memory.h"
#include "fs.h"

int serialize_inode(HeapData*, Inode);
int unserialize_inode(HeapData*, Inode*);

int serialize_superblock(HeapData*, Superblock);
int unserialize_superblock(HeapData*, Superblock*);



#endif