#ifndef FS_FS
#define FS_FS 

#include <stdint.h>
#include "memory.h"
#include "constants.h"

/* NOTE: Naming Conventions
	..._size: 		the size of something in bytes
	..._addr:		the block address: i.e. the block number.
					block number 0 is the super block
					bytes_from_start = addr * block_size

	data blocks 	Non reserved blocks used to store file data
					All blocks minus superblock, bitmaps and inode table
*/

typedef struct {
	HeapData data;

	// Block no. not stored in the block
	uint32_t block_number;
} Block;

typedef struct {
	uint32_t start_addr;
	uint32_t length; 
} BlockSequence; 

typedef struct {
	BlockSequence direct[6];
	uint32_t indirect;
	uint32_t double_indirect;
	uint32_t triple_indirect;
} DataStream;	


typedef struct {
	uint32_t magic_1;
	uint16_t version;

	uint32_t block_size;
	uint32_t num_blocks;
	uint32_t num_used_blocks;
	uint32_t num_data_blocks;

	uint32_t inode_size;
	uint32_t num_inodes;
	uint32_t num_used_inodes;

	uint32_t magic_2;

	uint32_t inode_bitmap_size;
	uint32_t inode_table_size;
	uint32_t data_block_bitmap_size;

	uint32_t inode_start_addr;
	uint32_t data_block_bitmap_addr;
	uint32_t data_blocks_start_addr;

	uint16_t flags;
} Superblock;

typedef struct {
	uint32_t magic;
	uint32_t inode_number;

	// For ownership information
	uint32_t uid;
	uint32_t gid;

	uint32_t flags;

	// Various times, just the unix timestamp
	uint32_t time_created;
	uint32_t time_last_modified;

	DataStream data;
} Inode;

typedef struct {
	uint32_t inode_number;
	uint8_t filename_length;

	// char* is not used - the filesystem doesn't care about the encoding.
	// filename does not need to be null terminated
	HeapData filename;
} DirectoryEntry;

// The directory is pointed to by the directory's inode
typedef HeapData Directory;
typedef HeapData Bitmap; 

int fs_create_superblock(Superblock*, uint64_t);
int fs_write_block(HeapData*, HeapData, int);
int fs_write_bitmap_bit(Bitmap*, int, int);
int fs_read_bitmap_bit(Bitmap*, int, int*);
int fs_add_directory_file(Directory*, DirectoryEntry);

#endif