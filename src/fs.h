#ifndef FS_FS
#define FS_FS 

#include <stdint.h>
#include <stdbool.h>

#include "memory.h"
#include "constants.h"
#include "bitmap.h"
#include "../../core/src/llist.h"

/* NOTE: Naming Conventions
	..._size: 		the size of something in bytes
	..._addr:		the block address: i.e. the block number.
					block number 0 is the super block
					bytes_from_start = addr * block_size

	data blocks 	Non reserved blocks used to store file data
					All blocks minus superblock, bitmaps and inode table
*/

#define ALLOC_FULL_FT_MAX 0.5



typedef HeapData InodeName;
typedef HeapData Directory;

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
	BlockSequence indirect;
	BlockSequence double_indirect;
	BlockSequence triple_indirect;
} DataStream;	

typedef struct {
	LList location;
	HeapData name;
} Path;

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
	uint32_t data_block_bitmap_size_bytes;
	uint32_t inode_bitmap_size_bytes;

	uint32_t inode_table_start_addr;
	uint32_t data_block_bitmap_addr;
	uint32_t data_blocks_start_addr;

	// bitmap_circular_loc - this is an optimisation for quickly finding 
	// a suitably long continuous region whilst the disk is fairly
	// unfragmented. The bits are initially allocated from bitmap[0]
	// to bitmap[N-1]. For the first run through it immediatly allows for 
	// continuous sections to be allocated. (NB preallocation will allow
	// for file growth in most situations, reducing fragmentation).

	uint32_t data_bitmap_circular_loc;

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

	// re preallocation: this is the disk space set aside for the file 
	// to allow for it to grow within the same continuous run/as close
	// as possible to the start. This improves disk access times (continuous
	// IO is very fast but seeking is one of the slowest possible
	// operations). This does not apply to flash based storage devices. 
	// In the future it could be interesting to optimise specifically
	// to flash storage devices (minimising small write operations (< 1 page)).

	uint64_t size;
	uint32_t preallocation_size;	// in use = file_size - preallocation_size

	DataStream data;
} Inode;

typedef struct {
	uint32_t inode_number;

	// char* is not used - the filesystem doesn't care about the encoding.
	// filename does not need to be null terminated
	HeapData name;
} DirectoryEntry;

typedef struct {
	// Lock the entire disk when a process is accessing it
	int is_locked;

	// Kept in memory for performance
	Superblock superblock;
	Bitmap inode_bitmap;
	Bitmap data_bitmap;
	// For an in memory disk
	HeapData data;

	// For using a file
	FILE* file;
	int size;
} Disk;

typedef struct {
	uint64_t addr;
} BlockAddress;


int fs_create_superblock(Superblock*, uint64_t);
int fs_allocate_blocks(Disk*, int, LList**); 
int fs_write_data_to_disk(Disk* disk, HeapData data, LList addresses, bool data_block);
int fs_write_inode(Disk disk, Inode inode, int* inode_number);
int fs_write_file(Disk* disk, Inode* inode, HeapData data, int* inode_number);
int fs_write_metadata(Disk disk);
Disk fs_create_filesystem(const char* name, int size, int* error);
HeapData fs_read_from_disk(Disk disk, LList addresses, bool data_block, int* error);
HeapData fs_read_from_disk_by_sequence(Disk disk, BlockSequence seq, bool data_block, int* error);
HeapData fs_read_from_disk(Disk disk, LList addresses, bool data_block, int* error);

#endif
