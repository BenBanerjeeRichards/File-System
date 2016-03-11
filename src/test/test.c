#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../fs.h"
#include "../util.h"
#include "../memory.h"
#include "../bitmap.h"
#include "../serialize.h"
#include "../stream.h"
#include "../directory.h"
#include "../../../core/src/llist.h"
#include "test.h"

int tests_run = 0;



Disk create_fragmented_disk() {
	const int size = MEGA * 310;	// ** Needs new disk IMPORTANT 
	Disk disk = { 0 };
	Superblock sb = { 0 };
	Bitmap data_bt = { 0 };
	disk.superblock = sb;
	disk.data_bitmap = data_bt;
	disk.size = size;

	fs_create_superblock(&disk.superblock, size);
	//disk_mount(&disk, "fragmented.bin");
	// TODO fix one off 
	mem_alloc(&disk.data_bitmap, disk.superblock.data_block_bitmap_size_bytes + 1);

	HeapData full_block = { 0 };
	mem_alloc(&full_block, BLOCK_SIZE);
	memset(full_block.data, 0xFF, full_block.size);

	int ret = 0;

	for (int i = 0; i < disk.superblock.num_data_blocks; i++) {
		if (i % 2 == 0) {
			ret = bitmap_write(&disk.data_bitmap, i, 1);
			if (ret != SUCCESS) {
				printf("Failed to write to bitmap at %i error code %i\n", i, ret);
				return disk;
			}

			// Uncomment to re-create disk AS WELL AS [A]
			
	/*		ret = disk_write(&disk, BLOCK_SIZE * (disk.superblock.data_blocks_start_addr + i), full_block);
			if (ret != SUCCESS) { 
				printf("Failed to write to disk at %i error code %i\n", i, ret);
				return disk;
			}  */
		}
	}

	mem_free(full_block);

	return disk;
}

static char* test_read_from_disk_by_seq() {
	Disk disk = { 0 };
	Superblock sb = { 0 };
	Bitmap db = { 0 };
	
	const int disk_size = MEGA;
	const char* fname = "test.bin";
	const int size_1 = 2;
	const int size_2 = 1;
	const int start_1 = 0;
	const int start_2 = 5;

	fs_create_superblock(&sb, disk_size);
	disk.superblock = sb;
	disk.data_bitmap = db;
	disk.size = disk_size;
	disk_mount(&disk, fname);

	HeapData data_1 = { 0 };
	mem_alloc(&data_1, BLOCK_SIZE * size_1);
	memset(data_1.data, 0xAA, BLOCK_SIZE * size_1);
	
	HeapData data_2 = { 0 };
	mem_alloc(&data_2, BLOCK_SIZE * size_2);
	memset(data_2.data, 0xBB, BLOCK_SIZE * size_2);

	HeapData data = { 0 };
	mem_alloc(&data, BLOCK_SIZE * (size_1 + size_2));
	memcpy(&data.data[0], data_1.data, size_1 * BLOCK_SIZE);
	memcpy(&data.data[BLOCK_SIZE * size_1], data_2.data, size_2 * BLOCK_SIZE);


	BlockSequence* seq_1 = malloc(sizeof(BlockSequence));
	seq_1->length = size_1;
	seq_1->start_addr = start_1;

	BlockSequence* seq_2 = malloc(sizeof(BlockSequence));
	seq_2->length = size_2;
	seq_2->start_addr = start_2;

	LList* addresses = llist_new();
	addresses->free_element = &free_element_standard;

	llist_insert(addresses, seq_1);
	llist_insert(addresses, seq_2);
	
	fs_write_data_to_disk(&disk, data, *addresses, 1);

	int ret = 0;
	HeapData read_1 = fs_read_from_disk_by_sequence(disk, *seq_1, 1, &ret);
	HeapData read_2 = fs_read_from_disk_by_sequence(disk, *seq_2, 1, &ret);
	int cmp = 0;
	cmp += memcmp(read_1.data, data_1.data, data_1.size);
	cmp += memcmp(read_2.data, data_2.data, data_2.size);

	mu_assert("[MinUnit][TEST] read data from disk seq: comparison failed", cmp == 0);

	fclose(disk.file);
	free(seq_1);
	free(seq_2);
	//llist_free(addresses);
	mem_free(data_1);
	mem_free(data_2);
	mem_free(data);
	remove(fname);
	return 0;
}

static char* test_write_data_to_disk_2() {
	const char* fname = "test_write_to_disk.bin";
	const int size_1 = 2;
	const int size_2 = 5;
	const int size_3 = 3;

	Disk disk = { 0 };
	disk.size = 128 * BLOCK_SIZE;
	disk_mount(&disk, fname);

	LList* addresses = llist_new();
	addresses->free_element = &free_element_standard;

	BlockSequence* seq1 = malloc(sizeof(BlockSequence));
	BlockSequence* seq2 = malloc(sizeof(BlockSequence));
	BlockSequence* seq3 = malloc(sizeof(BlockSequence));

	seq1->start_addr = 1;
	seq1->length = 2;

	seq2->start_addr = 5;
	seq2->length = 5;

	seq3->start_addr = 12;
	seq3->length = 3;

	llist_insert(addresses, seq1);
	llist_insert(addresses, seq2);
	llist_insert(addresses, seq3);

	HeapData data = { 0 };

	mem_alloc(&data, BLOCK_SIZE * (size_1 + size_2 + size_3));

	memset(&data.data[0], 0xAA, BLOCK_SIZE * size_1);
	memset(&data.data[BLOCK_SIZE * size_1], 0xBB, BLOCK_SIZE * size_2);
	memset(&data.data[BLOCK_SIZE * (size_2 + size_1)], 0xCC, BLOCK_SIZE * size_3);

	fs_write_data_to_disk(&disk, data, *addresses, 0);
	int ret = 0;
	HeapData data_1 = disk_read(disk, seq1->start_addr * BLOCK_SIZE, seq1->length * BLOCK_SIZE, &ret);
	HeapData data_2 = disk_read(disk, seq2->start_addr * BLOCK_SIZE, seq2->length * BLOCK_SIZE, &ret);
	HeapData data_3 = disk_read(disk, seq3->start_addr * BLOCK_SIZE, seq3->length * BLOCK_SIZE, &ret);

	int cmp = 0;
	cmp += memcmp(&data.data[0], data_1.data, seq1->length * BLOCK_SIZE);
	cmp += memcmp(&data.data[BLOCK_SIZE * size_1], data_2.data, seq2->length * BLOCK_SIZE);
	cmp += memcmp(&data.data[BLOCK_SIZE * (size_2 + size_1)], data_3.data, seq3->length * BLOCK_SIZE);

	mu_assert("[MinUnit][TEST] write data to disk: comparison failed", cmp == 0);

	mem_free(data);
	llist_free(addresses);
	fclose(disk.file);
	disk_remove(fname);
	
	return 0;
}

static char* test_file_disk_addressssing() {
	Disk disk = create_fragmented_disk();
	disk.file = fopen("fragmented.bin", "r+");

	const int allocation_size = 266310;
	LList* addresses;
	fs_allocate_blocks(&disk, allocation_size, &addresses);

	LListNode* current = addresses->head;
	for (int i = 0; i < addresses->num_elements; i++) {
		BlockSequence* seq = current->element;

		mu_assert("[MinUnit][TEST] file disk addressing: incorrect start address", 2 * i + 1 == seq->start_addr);
		mu_assert("[MinUnit][TEST] file disk addressing: incorrect length", 1 == seq->length);

		current = current->next;
	}

	Inode inode = { 0 };
	int ret = stream_write_addresses(&disk, &inode, *addresses);

	LList* indirect = stream_read_address_block(disk, inode.data.indirect, &ret);
	current = indirect->head;
	
	mu_assert("[MinUnit][TEST] file disk addressing: indirect: incorrect number of addresses", indirect->num_elements == 64);
	for (int i = 0; i < indirect->num_elements; i++) {
		BlockSequence* seq = current->element;
		mu_assert("[MinUnit][TEST] file disk addressing: indirect: unexpected start address", seq->start_addr == 13 + i * 2);
		mu_assert("[MinUnit][TEST] file disk addressing: indirect: unexpected length", seq->length == 1);

		current = current->next;
	}

	LList all_addresses = stream_read_addresses(disk, inode, &ret);

	LListNode* current_a = all_addresses.head;
	for (int i = 0; i < addresses->num_elements; i++) {
		int location = 2 * i + 1;	// Odd numbers for start_addr
		BlockSequence* seq = current_a->element;
		//printf("%i\n", seq->start_addr);
		mu_assert("[MinUnit][TEST] file disk addressing: incorrect start address", seq->start_addr == location);
		mu_assert("[MinUnit][TEST] file disk addressing: incorrect length", seq->length == 1);
		current_a = current_a->next;
	}

	//llist_free(&all_addresses);
	mem_free(disk.data_bitmap);
	fclose(disk.file);

	return 0;
}
static char* test_disk_io() {
	Disk disk = { 0 };
	disk.size = DISK_SIZE;

	disk_mount(&disk, "filesystem2.bin");

	HeapData data_1 = { 0 };
	HeapData data_2 = { 0 };
	HeapData data_3 = { 0 };
	HeapData data_4 = { 0 };


	mem_alloc(&data_1, 255);
	mem_alloc(&data_2, 5);
	mem_alloc(&data_3, 1);
	mem_alloc(&data_4, 10);

	for (int i = 0; i < 255; i++){
		mem_write(&data_1, i, i);
	}

	mem_write(&data_2, 0, 0xAA);
	mem_write(&data_2, 1, 0xBB);
	mem_write(&data_2, 2, 0xCC);
	mem_write(&data_2, 3, 0xDD);
	mem_write(&data_2, 4, 0xEE);

	mem_write(&data_3, 0, 0x17);

	disk_write(&disk, 0, data_3);
	disk_write(&disk, 255, data_1);
	disk_write(&disk, 14, data_2);

	int ret = disk_write(&disk, DISK_SIZE - 8, data_4);
	mu_assert("[MinUnit][TEST] disk io: Expected write to fail (data too large)", ret == ERR_DATA_TOO_LARGE);

	HeapData read_3 = disk_read(disk, 0, 1, &ret);
	HeapData read_1 = disk_read(disk, 255, 255, &ret);
	HeapData read_2 = disk_read(disk, 14, 5, &ret);
	int cmp = memcmp(read_1.data, data_1.data, read_1.size);
	cmp += memcmp(read_2.data, data_2.data, read_2.size);
	cmp += memcmp(read_3.data, data_3.data, read_3.size);
	mu_assert("[MinUnit][TEST] disk io: Comparison failed", cmp == 0);
	disk_unmount(disk);

	mem_free(data_1);
	mem_free(data_2);
	mem_free(data_3);
	mem_free(data_4);
	mem_free(read_1);
	mem_free(read_2);
	mem_free(read_3);
	disk_remove("filesystem2.bin");
	return 0;
}

static char* test_disk_io_2() {
	Disk disk = { 0 };
	disk.size = DISK_SIZE;
	disk_mount(&disk, FILESYSTEM_FILE_NAME);
	HeapData data = {0};
	mem_alloc(&data, 10);

	for (int i = 0; i < 10; i++) {
		if (i < 5) {
			mem_write(&data, i, 0xAA);
		}
		else {
			mem_write(&data, i, 0xBB);
		}
	}

	int ret = disk_write_offset(&disk, DISK_SIZE - 5 - 128, 128, data);
	HeapData read = disk_read_offset(disk, DISK_SIZE - 5 - 128, 128, 10, &ret);

	int cmp = memcmp(read.data, data.data, read.size);
	mu_assert("[MinUnit][TEST] disk io 2: Comparison failed", cmp == 0);


	disk_unmount(disk);
	mem_free(data);
	mem_free(read);
	disk_remove(FILESYSTEM_FILE_NAME);
	return 0;

}

static char* test_write_data_to_disk() {
	LList* list = llist_new();
	list->free_element = &free_element_standard;

	BlockSequence* seq1 = malloc(sizeof(BlockSequence));
	BlockSequence* seq2 = malloc(sizeof(BlockSequence));
	BlockSequence* seq3 = malloc(sizeof(BlockSequence));
	BlockSequence* seq4 = malloc(sizeof(BlockSequence));
	BlockSequence* seq5 = malloc(sizeof(BlockSequence));
	BlockSequence* seq6 = malloc(sizeof(BlockSequence));

	// Remember that seq.length refers the to block num, not byte. 

	seq1->start_addr = 5;
	seq1->length = 20;

	seq2->start_addr = 50;
	seq2->length = 2;

	seq3->start_addr = 60;
	seq3->length = 8;

	seq4->start_addr = 100;
	seq4->length = 1;

	seq5->start_addr = 110;
	seq5->length = 9;
	
	seq6->start_addr = 200;
	seq6->length = 10;

	llist_insert(list, seq1);
	llist_insert(list, seq2);
	llist_insert(list, seq3);
	llist_insert(list, seq4);
	llist_insert(list, seq5);
	llist_insert(list, seq6);

	HeapData data = { 0 };
	mem_alloc(&data, 50);
	
	HeapData disk_data = { 0 };
	mem_alloc(&disk_data, 512 * BLOCK_SIZE);

	Disk disk = { 0 };
	disk.data = disk_data;


	uint8_t values[50] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 
						  0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0xA1, 0xA2, 0xA3, 
						  0xB1, 0xB2, 0xB3, 0xB4, 0xC1, 0xC2, 0xC3, 0xC4, 0xD1, 
						  0xE1, 0xE2, 0xE3, 0xE4, 0xF1, 0xF2, 0xF3, 0xF4, 0x34, 
						  0x53, 0x12, 0x98, 0x23, 0x63, 0x73, 0x27, 0x73, 0xf9,
						  0xA5, 0xA6, 0xA7, 0xA9, 0xF7};

	for (int i = 0; i < 50; i++) {
		mem_write(&data, i, values[i]);
	}

	fs_write_data_to_disk(&disk, data, *list, 0);
	mem_free(data);
	mem_free(disk_data);
	llist_free(list);


	return 0;
}

static char* test_alloc_blocks_continuous() {
	Superblock superblock;
	fs_create_superblock(&superblock, DISK_SIZE);
	Bitmap block_bitmap = {0};
	mem_alloc(&block_bitmap, superblock.data_block_bitmap_size);
	int max = 128;
	for (int i = 0; i < 8 * max; i++) {
		if (rand() % 2 == 0) {
			bitmap_write(&block_bitmap, i, 1);
		}
	}

	mem_write(&block_bitmap, max, 0xFF);
	mem_write(&block_bitmap, max + 10, 0xFF);
	mem_write(&block_bitmap, max + 63 + 64, 0xFF);
	Disk disk = {0};
	disk.superblock = superblock;
	disk.data_bitmap = block_bitmap;
	LList* addresses;
	int ret = fs_allocate_blocks(&disk, 128 * 8, &addresses);
	
	BlockSequence* node = addresses->head->element;
	mu_assert("[MinUnit][TEST] alloc blocks continuous: incorrect alloc loaction (1)", node->start_addr == 256 * 8);

	llist_free(addresses);	
	mem_free(block_bitmap);
	return 0;
}

static char* test_alloc_blocks_non_continuous() {
	Superblock superblock;
	fs_create_superblock(&superblock, DISK_SIZE);
	Bitmap block_bitmap = {0};
	int ret = mem_alloc(&block_bitmap, superblock.data_block_bitmap_size);
	const int start_byte = 50300; // In filler after 10x1024

	// Create filler blocks [blue]
	HeapData filler_4000 = { 0 };
	mem_alloc(&filler_4000, 4000);
	memset(filler_4000.data, 0xFF, filler_4000.size);

	HeapData filler_5000 = { 0 };
	mem_alloc(&filler_5000, 5000);
	memset(filler_5000.data, 0xFF, filler_5000.size);

	HeapData filler_10000 = { 0 };
	mem_alloc(&filler_10000, 10000);
	memset(filler_10000.data, 0xFF, filler_10000.size);

	HeapData filler_6000 = { 0 };
	mem_alloc(&filler_6000, 6000);
	memset(filler_6000.data, 0xFF, filler_6000.size);

	HeapData filler_200 = { 0 };
	mem_alloc(&filler_200, 200);
	memset(filler_200.data, 0xFF, filler_200.size);

	// Write fillers at correct locations in bitmap
	int current_location = 0;
	ret = 0;
	for (int i = 0; i < 10; i++) {
		ret += mem_write_section(&block_bitmap, current_location, filler_4000);
		current_location += 4000 + 1024;
	}

	ret += mem_write_section(&block_bitmap, current_location, filler_5000);
	current_location += 5000 + 512;
	ret += mem_write_section(&block_bitmap, current_location, filler_5000);
	current_location += 5000 + 256;
	
	for (int i = 0; i < 2; i++) {
		ret += mem_write_section(&block_bitmap, current_location, filler_4000);
		current_location += 4000 + 128;
	}

	for (int i = 0; i < 2; i++) {
		ret += mem_write_section(&block_bitmap, current_location, filler_4000);
		current_location += 4000 + 64;
	}

	for (int i = 0; i < 4; i++) {
		ret += mem_write_section(&block_bitmap, current_location, filler_5000);
		current_location += 5000 + 32;
	}

	ret += mem_write_section(&block_bitmap, current_location, filler_10000);
	current_location += 10000 + 200;
	ret += mem_write_section(&block_bitmap, current_location, filler_6000);
	current_location += 6000 + 51;
	ret += mem_write_section(&block_bitmap, current_location, filler_200);
	current_location += 200 + 5;
	ret += mem_write_section(&block_bitmap, current_location, filler_200);
	current_location += 200;
	
	Disk disk = { 0 };
	disk.superblock = superblock;
	disk.data_bitmap = block_bitmap;
	
	LList* addresses;
	disk.superblock.data_bitmap_circular_loc = start_byte;

	ret = fs_allocate_blocks(&disk, 11776, &addresses);

	mu_assert("[MinUnit][TEST] alloc blocks non continuous: incorrect number of items in LL",
		addresses->num_elements == 23);

	LListNode* current = addresses->head;
	// Expected sizes 
	// Note that these sizes are the bytes alloc'd in bitmap NOT the number of blocks
	// blocks = 8 * bytes as 1 block/bit in bitmap
	int expected_sizes[23] = {512, 256, 128, 128, 64, 64, 32, 32, 32, 32, 
								200, 51, 5, 1024, 1024, 1024, 1024, 1024,
								1024, 1024, 1024, 1024, 1024 };

	int expected_locations[14] = {55240, 60752, 65008, 69136, 73264, 77328, 82392, 87424,
								92456, 97488, 107520, 113720, 113971, 4000};

	for (int i = 0; i < addresses->num_elements; i++) {
		BlockSequence* seq = current->element;

		mu_assert("[MinUnit][TEST] alloc blocks non continuous: mis matched expected and actual size",
			seq->length / 8 == expected_sizes[i]);

		if (i < 14) {
			mu_assert("[MinUnit][TEST] alloc blocks non continuous: mis matched expected and actual start addr",
				seq->start_addr / 8 == expected_locations[i]);
		}
		else {
			mu_assert("[MinUnit][TEST] alloc blocks non continuous: mis matched expected and actual start addr",
				seq->start_addr / 8 == 4000 + (4000 + 1024) * (i-13));
		}

		current = current->next;
	}

	llist_free(addresses);
	mem_free(disk.data_bitmap);
	mem_free(filler_10000);
	mem_free(filler_200);
	mem_free(filler_4000);
	mem_free(filler_5000);
	mem_free(filler_6000);


	return 0;
}

static char* test_find_continuous_run_length() {
	uint8_t bitmap_data[] = {0xFF, 0xFF, 0xFE, 0x0F, 0xFF, 0xFF, 0xFF};
	int run_len = 1 + 4;
	Bitmap bitmap = {0};
	int start_bit = 23;
	mem_alloc(&bitmap, 7);
	
	int len = 0;
	bitmap_find_continuous_run_length(bitmap, start_bit, &len);
	mu_assert("[MinUnit][TEST] find continuous run length: length incorrect", len == run_len);
	
	return 0;
}

static char* test_find_next_bitmap_block() {
	uint8_t bitmap_data[] = {0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF};
	int bit = 8 * 2 + 4 + 3;
	Bitmap bitmap = {0};
	int start = 4;
	mem_alloc(&bitmap, 7);
	memcpy(bitmap.data, bitmap_data, 7);

	int block_addr = 0;
	bitmap_find_block(bitmap, start, &block_addr);
	mu_assert("[MinUnit][TEST] find next bitmap block: incorrect block position [1]", bit == block_addr);
	block_addr = 0;
	bitmap_find_block(bitmap, 0, &block_addr);
	mu_assert("[MinUnit][TEST] find next bitmap block: incorrect block position [2]", bit == block_addr);
	block_addr = 0;
	bitmap_find_block(bitmap, 1, &block_addr);
	mu_assert("[MinUnit][TEST] find next bitmap block: incorrect block position [3]", bit == block_addr);
	
	mem_free(bitmap);
	return 0;
}

static char* test_next_dir_name() {
	char* p = "dir1/directory2/testing/structure";
	HeapData path = {0};
	HeapData name = {0};
	uint8_t expected1[] = {0x64, 0x69, 0x72, 0x31};
	uint8_t expected2[] = {0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x6f, 0x72, 0x79, 0x32};
	uint8_t expected3[] = {0x74, 0x65, 0x73, 0x74, 0x69, 0x6e, 0x67};
	uint8_t expected4[] = {0x73, 0x74, 0x72, 0x75, 0x63, 0x74, 0x75, 0x72, 0x65};

	util_string_to_heap(p, &path);

	int location = 0;
	int t = dir_find_next_path_name(path, location, &name);
	location += name.size + 1;	// Add one to get past forward slash	
	int ret = memcmp(name.data, expected1, name.size);
	mu_assert("[MinUnit][TEST] next dir name: dir name read is incorrect [1]", ret == 0);
	mu_assert("[MinUnit][TEST] next dir name: read blank dir name [1]", name.size != 0);
	mu_assert("[MinUnit][TEST] next dir name: length of directory incorrect [1]", name.size == 4);
	mem_free(name);

	dir_find_next_path_name(path, location, &name);
	location += name.size + 1;	// Add one to get past forward slash	
	ret = memcmp(name.data, expected2, name.size);
	mu_assert("[MinUnit][TEST] next dir name: dir name read is incorrect [2]", ret == 0);
	mu_assert("[MinUnit][TEST] next dir name: read blank dir name [2]", name.size != 0);
	mu_assert("[MinUnit][TEST] next dir name: length of directory incorrect [1]", name.size == 10);
	mem_free(name);
	
	dir_find_next_path_name(path, location, &name);
	location += name.size + 1;	// Add one to get past forward slash	
	ret = memcmp(name.data, expected3, name.size);
	mu_assert("[MinUnit][TEST] next dir name: dir name read is incorrect [3]", ret == 0);
	mu_assert("[MinUnit][TEST] next dir name: read blank dir name [3]", name.size != 0);
	mu_assert("[MinUnit][TEST] next dir name: length of directory incorrect [1]", name.size == 7);
	mem_free(name);

	dir_find_next_path_name(path, location, &name);
	location += name.size + 1;	// Add one to get past forward slash	
	ret = memcmp(name.data, expected4, name.size);
	mu_assert("[MinUnit][TEST] next dir name: dir name read is incorrect [4]", ret == 0);
	mu_assert("[MinUnit][TEST] next dir name: read blank dir name [4]", name.size != 0);
	mu_assert("[MinUnit][TEST] next dir name: length of directory incorrect [1]", name.size == 9);

	mem_free(name);
	mem_free(path);
	return 0;
}

static char* test_find_continuous_bitmap_run() {
	Bitmap bitmap = {0};
	mem_alloc(&bitmap, 10);
	int start_byte = 7;
	int length = 15;
	uint8_t bitmap_data[] = {0xFE, 0x00, 0xFF, 0xFE, 0xCC, 0x00, 0x06, 0xEE, 0x34, 0x8F};
	
	for (int i = 0; i < 10; i++) {
		mem_write(&bitmap, i, bitmap_data[i]);
	}
	
	int start = 0;
	bitmap_find_continuous_block_run(bitmap, length, start_byte, &start);
	
	mu_assert("[MinUnit][TEST] find bitmap run: incorrect start bit", start == 38);
	
	int ret = bitmap_find_continuous_block_run(bitmap, 17, 3, &start);
	mu_assert("[MinUnit][TEST] find bitmap run: found bitmap run, but expected error", ret == ERR_NO_BITMAP_RUN_FOUND); 

	mem_free(bitmap);
	return 0;
}

static char* test_find_continuous_bitmap_run_2()
{
	Bitmap bitmap = {0};
	mem_alloc(&bitmap, 8);

	int length = 2 * 8; // 2 bitmap bytes
	uint8_t bitmap_data[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x0E};
	
	for (int i = 0; i < 8; i++) {
		mem_write(&bitmap, i, bitmap_data[i]);
	}	
	int start_bit = 0;
	bitmap_find_continuous_block_run(bitmap, length, 0, &start_bit);
	mu_assert("[MinUnit][TEST] find bitmap run 2: incorrect start bit", start_bit == 32);
	mem_free(bitmap);	
	return 0;
}

static char* test_directory_get_inode_number() {
	// Add some test data
	Directory directory = {0};
	DirectoryEntry entry1 = {0};
	DirectoryEntry entry2 = {0};
	DirectoryEntry entry3 = {0};
	DirectoryEntry entry4 = {0};
	DirectoryEntry entry5 = {0};
	DirectoryEntry entry6 = {0};

	util_string_to_heap("This is a a file name.jpg", &entry1.name);
	entry1.inode_number = 988722354;
	util_string_to_heap("main.c", &entry2.name);
	entry2.inode_number = 673829463;
	util_string_to_heap("testing testing", &entry3.name);
	entry3.inode_number = 382647549;
	util_string_to_heap("** Thing thing **", &entry4.name);
	entry4.inode_number = 769823473;
	util_string_to_heap("The cat sat on the mat", &entry5.name);
	entry5.inode_number = 102938743;
	util_string_to_heap("filesystem test", &entry6.name);
	entry6.inode_number = 587493523;
	
	HeapData invalid = {0};
	util_string_to_heap("INVALID", &invalid);

	dir_add_entry(&directory, entry1);
	dir_add_entry(&directory, entry2);
	dir_add_entry(&directory, entry3);
	dir_add_entry(&directory, entry4);
	dir_add_entry(&directory, entry5);
	dir_add_entry(&directory, entry6);

	uint32_t inode_number = 0;
	dir_get_inode_number(directory, entry1.name, &inode_number);
	mu_assert("[MinUnit][FAIL] directory get inode: retrieved inode incorrect [1]", inode_number == 988722354);
	dir_get_inode_number(directory, entry2.name, &inode_number);
	mu_assert("[MinUnit][FAIL] directory get inode: retrieved inode incorrect [2]", inode_number == 673829463);
	dir_get_inode_number(directory, entry3.name, &inode_number);
	mu_assert("[MinUnit][FAIL] directory get inode: retrieved inode incorrect [3]", inode_number == 382647549);
	dir_get_inode_number(directory, entry4.name, &inode_number);
	mu_assert("[MinUnit][FAIL] directory get inode: retrieved inode incorrect [4]", inode_number == 769823473);
	dir_get_inode_number(directory, entry5.name, &inode_number);
	mu_assert("[MinUnit][FAIL] directory get inode: retrieved inode incorrect [5]", inode_number == 102938743);
	dir_get_inode_number(directory, entry6.name, &inode_number);
	mu_assert("[MinUnit][FAIL] directory get inode: retrieved inode incorrect [6]", inode_number == 587493523);
	int ret = dir_get_inode_number(directory, invalid, &inode_number);
	mu_assert("[MinUnit][FAIL] directory get inode: expected inode not found error", ret == ERR_INODE_NOT_FOUND);


	mem_free(entry1.name);
	mem_free(entry2.name);
	mem_free(entry3.name);
	mem_free(entry4.name);
	mem_free(entry5.name);
	mem_free(entry6.name);
	mem_free(directory);
	mem_free(invalid);
	return 0;
}

static char* test_directory_add_entry() {
	Directory directory = {0};
	HeapData filename = {0};
	HeapData filename2 = {0};
	DirectoryEntry entry = {0};
	DirectoryEntry entry2 = {0};

	uint8_t expected[] = {0x01, 0x41, 0xf0,0xf4, 0x0C, 0x48 ,0x65 ,0x6c ,0x6c ,0x6f ,0x20 ,0x57 ,0x6f ,0x72 ,0x6c ,0x64 ,0x21};

	uint8_t expected2[] = {0x01, 0x41, 0xf0,0xf4, 0x0C, 0x48 ,0x65 ,0x6c ,0x6c ,0x6f ,0x20 ,0x57 ,0x6f ,0x72 ,0x6c ,0x64 ,0x21, 0x83, 0xf7,0xbc, 0x82, 0x06, 0x6d ,0x61 ,0x69 ,0x6e ,0x2e ,0x63};

	int ret = 0;

	ret = util_string_to_heap("Hello World!", &filename);
	ret = util_string_to_heap("main.c", &filename2);

	entry.name = filename;
	entry.inode_number = 21098740;

	entry2.inode_number = 0x83f7bc82;
	entry2.name = filename2;

	dir_add_entry(&directory, entry);
	ret = memcmp(expected, directory.data, 17);
	mu_assert("[MinUnit][ERROR] directory add file: binary data produced incorrect [1]", ret == 0);


	ret = dir_add_entry(&directory, entry2);
	ret = memcmp(expected2, directory.data, sizeof(expected2));

	mu_assert("[MinUnit][ERROR] directory add file: binary data produced incorrect [2]", ret == 0);
	
	mem_free(directory);
	mem_free(entry.name);
	mem_free(entry2.name);

	return 0;
}
/*
static char* test_inode_serialization() {
	Inode inode = {0};
	Inode inode2 = {0};

	inode.magic = INODE_MAGIC;
	inode.inode_number = 3532482134;
	inode.uid = 293057934;
	inode.gid = 43835234;
	inode.flags = 2343;
	inode.time_created = 21349723;
	inode.time_last_modified = 65892342;
	inode.size = 0xAABBCCDDEEFF1122;
	inode.preallocation_size = 238947;
	inode.data.direct[0].start_addr = 123480973;
	inode.data.direct[1].start_addr = 90862343;
	inode.data.direct[2].start_addr = 20347563;
	inode.data.direct[3].start_addr = 674839201;
	inode.data.direct[4].start_addr = 394094538;
	inode.data.direct[5].start_addr = 758390124;

	inode.data.direct[0].length = 230847234;
	inode.data.direct[1].length = 982348672;
	inode.data.direct[2].length = 495873234;
	inode.data.direct[3].length = 789234873;
	inode.data.direct[4].length = 91283445;
	inode.data.direct[5].length = 294723485;


	inode.data.indirect =  238746910;
	inode.data.double_indirect = 5367283;
	inode.data.triple_indirect = 12655493;

	HeapData data;
	mem_alloc(&data, INODE_SIZE);
	serialize_inode(&data, inode);
	unserialize_inode(&data, &inode2);

	int ret = compare_inode(inode, inode2);

	mu_assert("[MinUnit][FAIL] inode serialization: inode changed by serialization", ret == 0);
	mem_free(data);
	return 0;

} */

static char* test_superblock_serialization() {
	Superblock superblock = { 0 };
	Superblock superblock2 = { 0 };

	fs_create_superblock(&superblock, 4096);

	// Manually set some values which would just be zero otherwise
	superblock.num_used_blocks = 523453;
	superblock.num_used_inodes = 2323;
	superblock.data_bitmap_circular_loc = 98734223;
	superblock.flags = 0xAABB;
	HeapData data = { 0 };
	mem_alloc(&data, 512);

	serialize_superblock(&data, superblock);
	unserialize_superblock(&data, &superblock2);

	mem_free(data);
	int ret = compare_superblock(superblock, superblock2);
	mu_assert("[MinUnit][FAIL] superblock serialization: superblock changed by serialization", ret == 0);
	return 0;
}

static char* test_div_round_up() {
	mu_assert("[MinUnit][FAIL] div round up: incorrect", div_round_up(6, 3) == 2);
	mu_assert("[MinUnit][FAIL] div round up: incorrect", div_round_up(6, 4) == 2);
	mu_assert("[MinUnit][FAIL] div round up: incorrect", div_round_up(23, 5) == 5);
	return 0;
}

static char* test_superblock_calculations() {
	Superblock superblock = {0};
	fs_create_superblock(&superblock, 4096 * 1024);

	mu_assert("[MinUnit][FAIL] superblock calculation: magic 1 incorrect", superblock.magic_1 == SUPERBLOCK_MAGIC_1);
	mu_assert("[MinUnit][FAIL] superblock calculation: magic 2 incorrect", superblock.magic_2 == SUPERBLOCK_MAGIC_2);
	mu_assert("[MinUnit][FAIL] superblock calculation: block size incorrect", superblock.block_size == 512);	// Assumption
	mu_assert("[MinUnit][FAIL] superblock calculation: num blocks incorrect", superblock.num_blocks == 8192);
	mu_assert("[MinUnit][FAIL] superblock calculation: inode size incorrect", superblock.inode_size == 128);	// Assumption	
	mu_assert("[MinUnit][FAIL] superblock calculation: num used blocks incorrect", superblock.num_used_blocks == 0);
	mu_assert("[MinUnit][FAIL] superblock calculation: num used inodes incorrect", superblock.num_used_inodes == 0);
	mu_assert("[MinUnit][FAIL] superblock calculation: num inodes incorrect", superblock.num_inodes == 512);
	mu_assert("[MinUnit][FAIL] superblock calculation: inode bitmap size incorrect", superblock.inode_bitmap_size == 512);
	mu_assert("[MinUnit][FAIL] superblock calculation: inode table size incorrect", superblock.inode_table_size == 64 * 1024);
	mu_assert("[MinUnit][FAIL] superblock calculation: num data blocks incorrect", superblock.num_data_blocks == 7167);
	mu_assert("[MinUnit][FAIL] superblock calculation: data block bitmap size incorrect", superblock.data_block_bitmap_size == 1024);
	mu_assert("[MinUnit][FAIL] superblock calculation: inode table start address incorrect", superblock.inode_table_start_addr == 2);
	mu_assert("[MinUnit][FAIL] superblock calculation: data block bitmap addr incorrect", superblock.data_block_bitmap_addr == 130);
	mu_assert("[MinUnit][FAIL] superblock calculation: data block start addr incorrect", superblock.data_blocks_start_addr == 132);
	return 0;
}

static char* test_bitmap_io() {
	HeapData block = {0};
	int ret = mem_alloc(&block, 8);
	mu_assert("[MinUnit][FAIL] bitmap io: failed to alloc block data", ret == SUCCESS);

	ret = 0;
	ret += bitmap_write(&block, 0, 1);
	ret += bitmap_write(&block, 1, 0);
	ret += bitmap_write(&block, 2, 0);
	ret += bitmap_write(&block, 3, 1);
	ret += bitmap_write(&block, 4, 0);
	ret += bitmap_write(&block, 5, 1);
	ret += bitmap_write(&block, 6, 1);
	ret += bitmap_write(&block, 7, 0);
	ret += bitmap_write(&block, 8, 1);
	ret += bitmap_write(&block, 9, 32);
	ret += bitmap_write(&block, 10, 0);
	ret += bitmap_write(&block, 11, 1);
	ret += bitmap_write(&block, 12, 1);
	ret += bitmap_write(&block, 13, 1);
	mu_assert("[MinUnit][FAIL] bitmap io: bitmap writing failed (mem error)", ret == SUCCESS);

	uint8_t byte1 = mem_read(block, 0, &ret);
	mu_assert("[MinUnit][FAIL] bitmap io: failed to read byte 1 (mem error)", ret == SUCCESS);
	uint8_t byte2 = mem_read(block, 1, &ret);
	mu_assert("[MinUnit][FAIL] bitmap io: failed to read byte 1 (mem error)", ret == SUCCESS);
	mu_assert("[MinUnit][FAIL] bitmap io: bitmap data incorrect (byte 1)", byte1 == 0x96);
	mu_assert("[MinUnit][FAIL] bitmap io: bitmap data incorrect (byte 2)", byte2 == 0xDC);

	byte1 = 0x00;
	byte2 = 0x00;

	for (int i = 0; i < 16; i++)
	{
		int v = bitmap_read(block, i, &ret);
		
		if (i < 8)
		{
			if (v) byte1 |= 1 << (7 - i);
		} else {
			if (v) byte2 |= 1 << (7 - (i - 8));
		}
	}

	mu_assert("[MinUnit][FAIL] bitmap io: bitmap data incorrect [read] (byte 1)", byte1 == 0x96);
	mu_assert("[MinUnit][FAIL] bitmap io: bitmap data incorrect [read] (byte 2)", byte2 == 0xDC);

	mem_free(block);

	return 0;

}

static char* all_tests() {
	mu_run_test(test_write_data_to_disk_2);

	mu_run_test(test_superblock_serialization);
	mu_run_test(test_superblock_calculations);
	mu_run_test(test_bitmap_io);
	//mu_run_test(test_inode_serialization);
	mu_run_test(test_directory_get_inode_number);
	mu_run_test(test_directory_add_entry);
	mu_run_test(test_find_continuous_bitmap_run);
	mu_run_test(test_next_dir_name);
	mu_run_test(test_find_next_bitmap_block);
	mu_run_test(test_alloc_blocks_continuous);
	mu_run_test(test_find_continuous_bitmap_run_2);
	//mu_run_test(test_alloc_blocks_non_continuous); TODO write better test
	mu_run_test(test_write_data_to_disk);
	mu_run_test(test_disk_io);
	mu_run_test(test_disk_io_2);
	mu_run_test(test_file_disk_addressssing);
	mu_run_test(test_div_round_up);
	mu_run_test(test_read_from_disk_by_seq);
	return 0;
}

void test_run_all(){
	printf("----------------------------------\n");
	printf("\033[1m[MinUnit] Running All Tests \033[0m\n");

    char *result = all_tests();
    if (result != 0) {
        printf("%s\n", result);
        printf("\033[1m[MinUnit] \e[31mFailed\e[0m \033[0m\n");
    }
    else {
        printf("\033[1m[MinUnit] \e[32mAll Tests Passed\e[0m \033[0m\n");
    }
    printf("\033[1m[MinUnit] Tests run: %d \033[0m\n", tests_run);
	printf("----------------------------------\n");

}
