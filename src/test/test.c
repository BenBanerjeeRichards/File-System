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
#include "../api.h"
#include "../constants.h"
#include "../disk.h"
#include "../../../core/src/llist.h"
#include "test.h"


int tests_run = 0;

// From http://stackoverflow.com/questions/2509679/how-to-generate-a-random-number-from-within-a-range
unsigned int rand_interval(unsigned int min, unsigned int max) {
	int r;
	const unsigned int range = 1 + max - min;
	const unsigned int buckets = RAND_MAX / range;
	const unsigned int limit = buckets * range;

	/* Create equal size buckets all in a row, then fire randomly towards
	* the buckets until you land in one of them. All buckets are equally
	* likely. If you land off the end of the line of buckets, try again. */
	do {
		r = rand();
	} while(r >= limit);

	return min + (r / buckets);
}


Disk create_fragmented_disk() {
	const int size = MEGA * 310;	
	Disk disk = { 0 };
	Superblock sb = { 0 };
	Bitmap data_bt = { 0 };
	disk.superblock = sb;
	disk.data_bitmap = data_bt;
	disk.size = size;
	
	fs_create_superblock(&disk.superblock, size);
	//disk_mount(&disk, "fragmented.bin");

	mem_alloc(&disk.data_bitmap, disk.superblock.data_block_bitmap_size_bytes);

	HeapData full_block = { 0 };
	mem_alloc(&full_block, BLOCK_SIZE);
	memset(full_block.data, 0xFF, full_block.size);

	int ret;

	for (int i = 0; i < disk.superblock.num_data_blocks; i++) {
		if (i % 2 == 0) {
			ret = bitmap_write(&disk.data_bitmap, i, 1);
			if (ret != SUCCESS) {
				printf("Failed to write to bitmap at %i error code %i\n", i, ret);
				return disk;
			}

			// Uncomment to re-create disk AS WELL AS [A]
			
			/* ret = disk_write(&disk, BLOCK_SIZE * (disk.superblock.data_blocks_start_addr + i), full_block);
			if (ret != SUCCESS) { 
				printf("Failed to write to disk at %i error code %i\n", i, ret);
				return disk;
			}   */
		}
	}

	mem_free(full_block);

	return disk;
}

Disk create_less_fragmented_disk() {
	const int size = MEGA * 310;	
	Disk disk = {0};
	Superblock sb = {0};
	Bitmap data_bt = {0};
	disk.superblock = sb;
	disk.data_bitmap = data_bt;
	disk.size = size;

	fs_create_superblock(&disk.superblock, size);
	disk_mount(&disk, "less_fragmented.bin");

	mem_alloc(&disk.data_bitmap, disk.superblock.data_block_bitmap_size_bytes + 1);

	HeapData full_block = {0};
	mem_alloc(&full_block, BLOCK_SIZE);
	memset(full_block.data, 0xFF, full_block.size);

	int current_block = 0;
	while(current_block < disk.superblock.num_data_blocks) {
		bitmap_write(&disk.data_bitmap, current_block, 1);
		//ret = disk_write(&disk, BLOCK_SIZE * (disk.superblock.data_blocks_start_addr + current_block), full_block);
		//if(ret != SUCCESS) printf("Error writing block %i to disk", current_block);
		current_block += 1 + rand_interval(0, 5);		//rand() in unit tests is a terrible idea FIXME
	}


	mem_free(full_block);

	return disk;
}

static char* test_dir_read_entry() {
	Directory directory = {0};
	HeapData filename = {0};
	HeapData filename2 = {0};
	HeapData filename3 = {0};
	DirectoryEntry entry = {0};
	DirectoryEntry entry2 = {0};
	DirectoryEntry entry3 = {0};

	int ret = 0;

	util_string_to_heap("Hello World.txt", &filename);
	util_string_to_heap("main.c", &filename2);
	util_string_to_heap("a nice file picture.jg", &filename3);

	entry.name = filename;
	entry.inode_number = 21098740;

	entry2.inode_number = 0x83f7bc82;
	entry2.name = filename2;

	entry2.inode_number = 213987;
	entry2.name = filename3;


	dir_add_entry(&directory, entry);
	dir_add_entry(&directory, entry2);
	dir_add_entry(&directory, entry3);

	int current = 0;
	DirectoryEntry e1 = dir_read_next_entry(directory, current, &ret);
	current += e1.name.size;
	current += 5;
	
	mu_assert("[MinUnit][TEST] test dir read next entry: incorrect entry inode number (1)", e1.inode_number == entry.inode_number);
	
	int cmp = memcmp(entry.name.data, e1.name.data, entry.name.size);
	mu_assert("[MinUnit][TEST] test dir read next entry: incorrect file name (1)", cmp == 0);

	DirectoryEntry e2 = dir_read_next_entry(directory, current, &ret);
	current += e2.name.size;
	current += 5;

	mu_assert("[MinUnit][TEST] test dir read next entry: incorrect entry inode number (2)", e2.inode_number == entry2.inode_number);
	cmp = memcmp(entry2.name.data, e2.name.data, entry2.name.size);
	mu_assert("[MinUnit][TEST] test dir read next entry: incorrect file name (2)", cmp == 0);


	DirectoryEntry e3 = dir_read_next_entry(directory, current, &ret);
	
	mu_assert("[MinUnit][TEST] test dir read next entry: incorrect entry inode number (3)", e3.inode_number == entry3.inode_number);
	cmp = memcmp(entry3.name.data, e3.name.data, entry3.name.size);
	mu_assert("[MinUnit][TEST] test dir read next entry: incorrect file name (3)", cmp == 0);

	
	mem_free(directory);
	mem_free(entry.name);
	mem_free(entry2.name);


	return 0;
}

/* This is an example of a bad unit test as it tests multiple units of functionality 
in a single test. May refactor later on, leaving it in here for exemplification for 
my assignment work.

It tests:	
	- api_list_directory
	- api_delete_file
	- api_create_dir
*/

// TODO refactor this test
static char* test_api_directory_handling() {
	// Create 'Test Directorty/file1.txt, file2.txt'
	int ret = 0;
	Disk disk = fs_create_filesystem("testrd.bin", MEGA, &ret);

	HeapData path = {0};
	util_string_to_heap("", &path);

	HeapData test_dir_name = {0};
	util_string_to_heap("Test Directory", &test_dir_name);
	
	HeapData fs_1 = {0};
	HeapData fs_2 = {0};
	HeapData fs_3 = {0};
	util_string_to_heap("Test Directory/Test File 1.txt", &fs_1);
	util_string_to_heap("Test Directory/Test File 2.txt", &fs_2);
	util_string_to_heap("Test Directory/Test File 3.txt", &fs_3);

	HeapData name_1 = {0};
	HeapData name_2 = {0};
	HeapData name_3 = {0};
	util_string_to_heap("Test File 1.txt", &name_1);
	util_string_to_heap("Test File 2.txt", &name_2);
	util_string_to_heap("Test File 3.txt", &name_3);


	Permissions perm = {0};

	ret = api_create_dir(&disk, path, path);
	ret = api_create_dir(&disk, path, test_dir_name);

	ret = api_create_file(disk, perm, fs_1);
	ret = api_create_file(disk, perm, fs_2);
	ret = api_create_file(disk, perm, fs_3);

	LList* items;
	api_list_directory(disk, test_dir_name, &items);

	mu_assert("[MinUnit][TEST] api remove entry: list incorrect number of items (1)", items->num_elements == 3);
	int cmp = memcmp(name_1.data, (*(FileDetails*)items->head->element).name.data, name_1.size);
	mu_assert("[MinUnit][TEST] api remove entry: list incorrect item  (1:1)", cmp == 0);
	cmp = memcmp(name_2.data, (*(FileDetails*)items->head->next->element).name.data, name_2.size);
	mu_assert("[MinUnit][TEST] api remove entry: list incorrect item  (1:2)", cmp == 0);
	cmp = memcmp(name_3.data, (*(FileDetails*)items->head->next->next->element).name.data, name_3.size);
	mu_assert("[MinUnit][TEST] api remove entry: list incorrect item  (1:3)", cmp == 0);

	llist_free(items);

	ret = api_delete_file(&disk, fs_2);
	api_list_directory(disk, test_dir_name, &items);

	mu_assert("[MinUnit][TEST] api remove entry: list incorrect number of items (2)", items->num_elements == 2);
	cmp = memcmp(name_1.data, (*(FileDetails*)items->head->element).name.data, name_1.size);
	mu_assert("[MinUnit][TEST] api remove entry: list incorrect item  (2:1)", cmp == 0);
	cmp = memcmp(name_3.data, (*(FileDetails*)items->head->next->element).name.data, name_3.size);
	mu_assert("[MinUnit][TEST] api remove entry: list incorrect item  (2:1)", cmp == 0);

	llist_free(items);

	ret = api_delete_file(&disk, fs_1);
	api_list_directory(disk, test_dir_name, &items);

	mu_assert("[MinUnit][TEST] api remove entry: list incorrect number of items (1)", items->num_elements == 1);
	cmp = memcmp(name_3.data, (*(FileDetails*)items->head->element).name.data, name_3.size);
	mu_assert("[MinUnit][TEST] api remove entry: list incorrect item  (3:1)", cmp == 0);

	llist_free(items);

	ret = api_delete_file(&disk, fs_3);
	api_list_directory(disk, test_dir_name, &items);
	mu_assert("[MinUnit][TEST] api remove entry: list incorrect number of items (3)", items->num_elements == 0);
	llist_free(items);


	mem_free(test_dir_name);
	mem_free(fs_1);
	mem_free(fs_2);
	mem_free(fs_3);
	mem_free(name_1);
	mem_free(name_2);
	mem_free(name_3);
	return 0;
}

static char* test_api_create_dir() {
	// TODO
	return 0;
}

static char* test_rw_1() {
	int ret = 0;
	Disk disk = fs_create_filesystem("testfs.bin", MEGA, &ret);

	// Create a file
	Directory root_dir = {0};
	Directory test_dir = {0};

	DirectoryEntry test_dir_entry = {0};
	DirectoryEntry file_entry = {0};

	HeapData test_dir_name = {0};
	util_string_to_heap("Test Directory", &test_dir_name);
	test_dir_entry.name = test_dir_name;
	
	HeapData file_name = {0};
	util_string_to_heap("File 2.txt", &file_name);
	file_entry.name = file_name;
	file_entry.inode_number = 0xBB;
	test_dir_entry.inode_number = 1;
	
	dir_add_entry(&root_dir, test_dir_entry);
	dir_add_entry(&test_dir, file_entry);

	Inode root_inode = {0};
	root_inode.size = root_dir.size;
	root_inode.magic = INODE_MAGIC;

	Inode test_dir_inode = {0};
	test_dir_inode.size = test_dir.size;
	test_dir_inode.magic = INODE_MAGIC;

	int root_inode_num = 0;
	int test_dir_inode_num = 0;

	ret = fs_write_file(&disk, &root_inode, root_dir, &root_inode_num);
	ret = fs_write_file(&disk, &test_dir_inode, test_dir, &test_dir_inode_num);

	// Create a Test File
	char* file_contents_str = "To be, or not to be: that is the question:Whether 'tis nobler in the mind to sufferThe slings and arrows of outrageous fortune,Or to take arms against a sea of troubles,And by opposing end them? To die: to sleep;No more; and by a sleep to say we endThe heart-ache and the thousand natural shocksThat flesh is heir to, 'tis a consummationDevoutly to be wish'd. To die, to sleep;To sleep: perchance to dream: ay, there's the rub;For in that sleep of death what dreams may comeWhen we have shuffled off this mortal coil,Must give us pause: there's the respectThat makes calamity of so long life;For who would bear the whips and scorns of time,The oppressor's wrong, the proud man's contumely,The pangs of despised love, the law's delay,The insolence of office and the spurnsThat patient merit of the unworthy takes,When he himself might his quietus makeWith a bare bodkin? who would fardels bear,To grunt and sweat under a weary life,But that the dread of something after death,The undiscover'd country from whose bournNo traveller returns, puzzles the willAnd makes us rather bear those ills we haveThan fly to others that we know not of?Thus conscience does make cowards of us all;And thus the native hue of resolutionIs sicklied o'er with the pale cast of thought,And enterprises of great pith and momentWith this regard their currents turn awry,And lose the name of action.Soft you now!The fair Ophelia! Nymph, in thy orisonsBe all my sins remember'd.";

	HeapData file_contents = {0};
	util_string_to_heap(file_contents_str, &file_contents);

	HeapData path = {0};
	util_string_to_heap("Test Directory/Some English.txt", &path);


	Permissions perm = {0};
	api_create_file(disk, perm, path);
	HeapData read = {0};
	api_write_to_file(disk, path, file_contents);
	ret = api_read_all_from_file(disk, 2, &read);

	int cmp = memcmp(file_contents.data, read.data, file_contents.size);
	mu_assert("[MinUnit][TEST] test rw 1: read data incorrect", cmp == 0);

	return 0;
}

static char* test_fill_unused_allocated_data() {
	Disk disk = create_fragmented_disk();
	disk.file = fopen("fragmented.bin", "r+");

	const int allocation_size_blocks = 4;		
	const int allocation_size_bytes = 1797;		// 3.5 blocks + 5 bytes
	const int append_size = 512;		
	LList* addresses;
	fs_allocate_blocks(&disk, allocation_size_blocks, &addresses);

	Inode inode = {0};
	int ret = stream_write_addresses(&disk, &inode, *addresses);

	// Write the data to the disk
	HeapData data = {0};
	mem_alloc(&data, allocation_size_bytes);
	memset(data.data, 0xBB, allocation_size_bytes);
	fs_write_data_to_disk(&disk, data, *addresses, true);
	inode.size = allocation_size_bytes;

	// Create memory to append
	HeapData append_data = {0};
	mem_alloc(&append_data, append_size);
	memset(append_data.data, 0xDD, 256 - 5);
	memset(&append_data.data[256 - 5], 0xEE, 256 + 5);

	HeapData remaining_data = {0};
	fs_fill_unused_allocated_data(&disk, &inode, append_data, &remaining_data);

	mu_assert("[MinUnit][TEST] fill unused: remaining data incorrect size", remaining_data.size == 256 + 5);

	// Now check the data was written correctlty
	addresses = stream_read_addresses(disk, inode, &ret);

	// Read data from the last item
	BlockSequence* seq = addresses->tail->element;

	HeapData block = fs_read_from_disk_by_sequence(disk, *seq, true, &ret);

	// Create memory block 
	HeapData expected = {0};
	mem_alloc(&expected, 512);

	memset(expected.data, 0xBB, 256 - 5);
	memset(&expected.data[256 - 5], 0xDD, 256 + 5);

	// Compare
	int cmp = memcmp(block.data, block.data, 512);
	mu_assert("[MinUnit][TEST] fill unused: final block comparison failed", cmp == 0);

	// Check the remaining data
	for(int i = 0; i < remaining_data.size; i++) {
		mu_assert("[MinUnit][TEST] fill unused: remaining data incorrect", remaining_data.data[i] == 0xEE);
	}

	mem_free(expected);
	mem_free(block);
	mem_free(append_data);
	mem_free(disk.data_bitmap);
	return 0;
}

static char* test_read_inode() {
	int ret = 0;
	Disk disk = fs_create_filesystem("testreadinode.bin", MEGA, &ret);

	// Create some sample inodes
	Inode inode_1 = {0};
	Inode inode_2 = {0};
	Inode inode_3 = {0};

	inode_1.magic = inode_2.magic = inode_3.magic = INODE_MAGIC;

	inode_1.time_created = 123456789;
	inode_2.time_created = 987654321;
	inode_3.time_created = 657483929;

	inode_1.time_last_modified = 65465452;
	inode_2.time_last_modified = 34534856;
	inode_3.time_last_modified = 74136955;

	inode_1.flags = 23;
	inode_2.flags = 123982;
	inode_3.flags = 123;

	inode_1.gid = 1;
	inode_2.gid = 2;
	inode_3.gid = 3;

	inode_1.uid = 4;
	inode_2.uid = 5;
	inode_3.uid = 5;

	inode_1.preallocation_size = 64;
	inode_2.preallocation_size = 65;
	inode_3.preallocation_size = 66;

	inode_1.size = 23;
	inode_2.size = 523;
	inode_3.size = 12309;

	inode_1.data.direct[0].start_addr = 123480973;
	inode_1.data.direct[1].start_addr = 90862343;
	inode_1.data.direct[2].start_addr = 20347563;
	inode_1.data.direct[3].start_addr = 674839201;
	inode_1.data.direct[4].start_addr = 394094538;
	inode_1.data.direct[5].start_addr = 758390124;

	inode_2.data.direct[0].length = 230847234;
	inode_2.data.direct[1].length = 982348672;
	inode_2.data.direct[2].length = 495873234;
	inode_2.data.direct[3].length = 789234873;
	inode_2.data.direct[4].length = 91283445;
	inode_2.data.direct[5].length = 294723485;

	// Write the inodes to disk
	int inode_1_num = 0;
	int inode_2_num = 0;
	int inode_3_num = 0;
	fs_write_inode(disk, &inode_1, &inode_1_num);
	fs_write_inode(disk, &inode_2, &inode_2_num);
	fs_write_inode(disk, &inode_3, &inode_3_num);

	// Read the inodes again
	Inode inode_read_1 = fs_read_inode(disk, inode_1_num, &ret);
	Inode inode_read_2 = fs_read_inode(disk, inode_2_num, &ret);
	Inode inode_read_3 = fs_read_inode(disk, inode_3_num, &ret);

	// Compare
	int cmp = compare_inode(inode_1, inode_read_1);
	mu_assert("[MinUnit][TEST] read inode: inode comparison failed [1]", cmp == 0);

	cmp = compare_inode(inode_2, inode_read_2);
	mu_assert("[MinUnit][TEST] read inode: inode comparison failed [2]", cmp == 0);

	cmp = compare_inode(inode_3, inode_read_3);
	mu_assert("[MinUnit][TEST] read inode: inode comparison failed [3]", cmp == 0);

	disk_unmount(disk);
	disk_remove(disk.filename);
	disk_free(disk);
	return 0;
}

static char* test_append_data_to_disk() {
	Disk disk = create_fragmented_disk();

	disk.file = fopen("fragmented.bin", "r+");

	const int allocation_size = 3000; //66310
	LList* addresses;
	fs_allocate_blocks(&disk, allocation_size, &addresses);

	Inode inode = {0};
	int ret = stream_write_addresses(&disk, &inode, *addresses);
	
	// ---------------

	const int allocation_size_2 = 200000;
	LList* addresses_2;
	fs_allocate_blocks(&disk, allocation_size_2, &addresses_2);
	stream_append_to_addresses(disk, &inode, *addresses_2);

	LList* read = stream_read_addresses(disk, inode, &ret);

	llist_append(addresses, *addresses_2);
	bool cmp = llist_is_equal(*addresses, *read, &compare_block_sequence);
	mu_assert("[MinUnit][TEST] append to disk: new llist incorrect", cmp);


	llist_free(addresses);
	mem_free(disk.data_bitmap);
	fclose(disk.file);

	return 0;
}

static char* test_metedata_load_and_store() {
	const char* name = "testloadstore.bin";
	int ret = 0;

	Disk disk = fs_create_filesystem(name, KILO, &ret);

	// Fill out some more information
	disk.superblock.num_used_blocks = 0xFE23;
	disk.superblock.data_bitmap_circular_loc = 34;
	disk.superblock.flags = 53234;
	disk.superblock.num_used_inodes = 23894;

	for(int i = 0; i < disk.superblock.num_data_blocks; i++) {
		bitmap_write(&disk.data_bitmap, i, i % 13);
	}

	for(int i = 0; i < disk.superblock.num_inodes; i++) {
		bitmap_write(&disk.inode_bitmap, i, i % 15);
	}

	// Deep copy disk for comparison later on
	// This could be a useful util function
	Disk disk_1 = {0};
	disk_1.superblock.block_size = disk.superblock.block_size;
	disk_1.superblock.data_bitmap_circular_loc = disk.superblock.data_bitmap_circular_loc;
	disk_1.superblock.data_blocks_start_addr = disk.superblock.data_blocks_start_addr;
	disk_1.superblock.data_block_bitmap_addr = disk.superblock.data_block_bitmap_addr;
	disk_1.superblock.data_block_bitmap_size = disk.superblock.data_block_bitmap_size;
	disk_1.superblock.data_block_bitmap_size_bytes = disk.superblock.data_block_bitmap_size_bytes;
	disk_1.superblock.flags = disk.superblock.flags;
	disk_1.superblock.inode_bitmap_size = disk.superblock.inode_bitmap_size;
	disk_1.superblock.inode_bitmap_size_bytes = disk.superblock.inode_bitmap_size_bytes;
	disk_1.superblock.inode_size = disk.superblock.inode_size;
	disk_1.superblock.inode_table_size = disk.superblock.inode_table_size;
	disk_1.superblock.inode_table_start_addr = disk.superblock.inode_table_start_addr;
	disk_1.superblock.magic_1 = disk.superblock.magic_1;
	disk_1.superblock.magic_2 = disk.superblock.magic_2;
	disk_1.superblock.num_blocks = disk.superblock.num_blocks;
	disk_1.superblock.num_data_blocks = disk.superblock.num_data_blocks;
	disk_1.superblock.num_inodes = disk.superblock.num_inodes;
	disk_1.superblock.num_used_blocks = disk.superblock.num_used_blocks;
	disk_1.superblock.num_used_inodes = disk.superblock.num_used_inodes;
	disk_1.superblock.version = disk.superblock.version;

	Bitmap db = {0};
	Bitmap ib = {0};
	disk.data_bitmap = db;
	disk.inode_bitmap = ib;
	mem_alloc(&disk_1.data_bitmap, disk.data_bitmap.size);
	mem_alloc(&disk_1.inode_bitmap, disk.inode_bitmap.size);
	memcpy(disk_1.data_bitmap.data, disk.data_bitmap.data, disk.data_bitmap.size);
	memcpy(disk_1.inode_bitmap.data, disk.inode_bitmap.data, disk.inode_bitmap.size);

	fs_write_metadata(disk);
	fs_read_metadata(&disk);

	int cmp = compare_superblock(disk_1.superblock, disk.superblock);
	mu_assert("[MinUnit][TEST] metadata io: failed superblock comparison", cmp == 0);

	cmp = memcmp(disk.data_bitmap.data, disk_1.data_bitmap.data, disk.data_bitmap.size);
	mu_assert("[MinUnit][TEST] metadata io: failed data bitmap comparison", cmp == 0);
	
	cmp = memcmp(disk.inode_bitmap.data, disk_1.inode_bitmap.data, disk.inode_bitmap.size);
	mu_assert("[MinUnit][TEST] metadata io: failed inode bitmap comparison", cmp == 0);


	disk_unmount(disk);
	disk_remove(name);
	return 0;
}

static char* test_write_inode() {
	const int size = MEGA;
	const char* fname = "testinode.bin";
	int ret = 0;

	Disk disk = {0};
	Superblock sb = {0};
	Bitmap data_bt = {0};

	disk.superblock = sb;
	disk.data_bitmap = data_bt;
	disk.size = size;

	fs_create_superblock(&disk.superblock, size);
	disk_mount(&disk, fname);
	mem_alloc(&disk.inode_bitmap, disk.superblock.inode_bitmap_size_bytes);

	Inode inode_1 = {0};
	Inode inode_2 = {0};
	Inode inode_3 = {0};
	
	inode_1.magic = inode_2.magic = inode_3.magic = INODE_MAGIC;
	
	inode_1.time_created = 123456789;
	inode_2.time_created = 987654321;
	inode_3.time_created = 657483929;

	inode_1.time_last_modified = 65465452;
	inode_2.time_last_modified = 34534856;
	inode_3.time_last_modified = 74136955;

	inode_1.flags = 23;
	inode_2.flags = 123982;
	inode_3.flags = 123;

	inode_1.gid = 1;
	inode_2.gid = 2;
	inode_3.gid = 3;

	inode_1.uid = 4;
	inode_2.uid = 5;
	inode_3.uid = 5;

	inode_1.preallocation_size = 64;
	inode_2.preallocation_size = 65;
	inode_3.preallocation_size = 66;

	inode_1.size = 23;
	inode_2.size = 523;
	inode_3.size = 12309;

	inode_1.data.direct[0].start_addr = 123480973;
	inode_1.data.direct[1].start_addr = 90862343;
	inode_1.data.direct[2].start_addr = 20347563;
	inode_1.data.direct[3].start_addr = 674839201;
	inode_1.data.direct[4].start_addr = 394094538;
	inode_1.data.direct[5].start_addr = 758390124;

	inode_2.data.direct[0].length = 230847234;
	inode_2.data.direct[1].length = 982348672;
	inode_2.data.direct[2].length = 495873234;
	inode_2.data.direct[3].length = 789234873;
	inode_2.data.direct[4].length = 91283445;
	inode_2.data.direct[5].length = 294723485;

	int num = 0;
	fs_write_inode(disk, &inode_1, &num);
	mu_assert("[MinUnit][TEST] write inode: incorrect inode number [1]", num == 0);
	
	fs_write_inode(disk, &inode_2, &num);
	mu_assert("[MinUnit][TEST] write inode: incorrect inode number [2]", num == 1);

	fs_write_inode(disk, &inode_3, &num);
	mu_assert("[MinUnit][TEST] write inode: incorrect inode number [3]", num == 2);

	HeapData inode_read_1_data = disk_read(disk, disk.superblock.inode_table_start_addr * BLOCK_SIZE, INODE_SIZE, &ret);
	HeapData inode_read_2_data = disk_read(disk, (disk.superblock.inode_table_start_addr + INODE_SIZE) * BLOCK_SIZE, INODE_SIZE, &ret);
	HeapData inode_read_3_data = disk_read(disk, (disk.superblock.inode_table_start_addr + 2 * INODE_SIZE) * BLOCK_SIZE, INODE_SIZE, &ret);

	Inode inode_read_1 = {0};
	Inode inode_read_2 = {0};
	Inode inode_read_3 = {0};

	unserialize_inode(&inode_read_1_data, &inode_read_1);
	unserialize_inode(&inode_read_2_data, &inode_read_2);
	unserialize_inode(&inode_read_3_data, &inode_read_3);

	int cmp = compare_inode(inode_1, inode_read_1);
	mu_assert("[MinUnit][TEST] write inode: inode does not match unserialized inode [1]", cmp == 0);
	
	cmp = compare_inode(inode_1, inode_read_1);
	mu_assert("[MinUnit][TEST] write inode: inode does not match unserialized inode [2]", cmp == 0);
	
	cmp = compare_inode(inode_1, inode_read_1);
	mu_assert("[MinUnit][TEST] write inode: inode does not match unserialized inode [3]", cmp == 0);

	disk_unmount(disk);
	remove(fname);

	return 0;
}

static char* test_directory_traversal() {
	const int size = MEGA;
	const char* fname = "testtrav.bin";
	int ret;

	Disk disk = {0};
	Superblock sb = {0};
	Bitmap data_bt = {0};
	
	disk.superblock = sb;
	disk.data_bitmap = data_bt;
	disk.size = size;

	fs_create_superblock(&disk.superblock, size);
	disk_mount(&disk, fname);
	mem_alloc(&disk.data_bitmap, disk.superblock.data_block_bitmap_size_bytes);
	mem_alloc(&disk.inode_bitmap, disk.superblock.inode_bitmap_size_bytes);

	Directory root = {0};
	Directory test_dir_a = {0};
	Directory test_sub_a = {0};

	Inode root_inode = {0};
	Inode test_dir_a_inode = {0};
	Inode test_sub_a_inode = {0};
	Inode testfile_a_inode = {0};
	Inode testfile_b_inode = {0};

	root_inode.flags &= INODE_FLAG_IS_DIR;
	test_dir_a_inode.flags &= INODE_FLAG_IS_DIR;
	test_sub_a_inode.flags &= INODE_FLAG_IS_DIR;

	DirectoryEntry test_dir_a_entry = {0};
	DirectoryEntry test_sub_a_entry = {0};
	DirectoryEntry testfile_a_entry = {0};
	DirectoryEntry testfile_b_entry = {0};

	HeapData test_dir_a_name = {0};
	HeapData test_sub_a_name = {0};
	HeapData testfile_a_name = {0};
	HeapData testfile_b_name = {0};

	util_string_to_heap("test dir A", &test_dir_a_name);
	util_string_to_heap("test sub dir A", &test_sub_a_name);
	util_string_to_heap("Test File A.txt", &testfile_a_name);
	util_string_to_heap("Test File B.txt", &testfile_b_name);

	test_dir_a_entry.inode_number = 1;
	test_sub_a_entry.inode_number = 2;
	testfile_a_entry.inode_number = 3;
	testfile_b_entry.inode_number = 50;

	test_dir_a_entry.name = test_dir_a_name;
	test_sub_a_entry.name = test_sub_a_name;
	testfile_a_entry.name = testfile_a_name;
	testfile_b_entry.name = testfile_b_name;

	dir_add_entry(&root, test_dir_a_entry);
	dir_add_entry(&test_dir_a, test_sub_a_entry);
	dir_add_entry(&test_sub_a, testfile_a_entry);
	dir_add_entry(&test_sub_a, testfile_b_entry);
	
	root_inode.size = root.size;
	test_dir_a_inode.size = test_dir_a.size;
	testfile_a_inode.size = 0;
	testfile_b_inode.size = 0;
	root_inode.magic = INODE_MAGIC;
	test_dir_a_inode.magic = INODE_MAGIC;
	testfile_a_inode.magic = INODE_MAGIC;
	test_sub_a_inode.magic = INODE_MAGIC;

	int num = 0;
	// Write directories to disk (directories are just special files)
	ret = 0;
	ret += fs_write_file(&disk, &root_inode, root, &num);
	ret += fs_write_file(&disk, &test_dir_a_inode, test_dir_a, &num);
	test_sub_a_entry.inode_number = num;
	ret += fs_write_file(&disk, &test_sub_a_inode, test_sub_a, &num);
	testfile_a_entry.name = testfile_a_name;

	HeapData path_1 = {0};
	HeapData path_2 = {0};

	util_string_to_heap("test dir A/test sub dir A", &path_1);
	util_string_to_heap("test dir A/test sub dir A/Test File B.txt", &path_2);

	DirectoryEntry directory_1;
	dir_get_directory(disk, path_1, root, &directory_1);
	mu_assert("[MinUnit][TEST] test directory traversal: incorrect inode number [1]", directory_1.inode_number == 2);
	DirectoryEntry directory_2;
	dir_get_directory(disk, path_2, root, &directory_2);
	mu_assert("[MinUnit][TEST] test directory traversal: incorrect inode number [2]", directory_2.inode_number == 50);



	disk_unmount(disk);
	disk_remove(fname);
	// TODO free directories
	mem_free(test_dir_a);
	mem_free(testfile_a_name);
	mem_free(testfile_b_name);
	mem_free(disk.inode_bitmap);
	mem_free(disk.data_bitmap);
	return 0;
}


static char* test_read_from_disk() {
	const int size = MEGA;
	const char* fname = "testread.bin";
	int ret = 0;
	Disk disk = {0};
	Superblock sb = {0};
	Bitmap data_bt = {0};
	disk.superblock = sb;
	disk.data_bitmap = data_bt;
	disk.size = size;

	fs_create_superblock(&disk.superblock, size);
	disk_mount(&disk, fname);
	mem_alloc(&disk.data_bitmap, disk.superblock.data_block_bitmap_size_bytes + 1);

	HeapData data_1 = {0};
	HeapData data_2 = {0};
	HeapData data_3 = {0};
	mem_alloc(&data_1, BLOCK_SIZE);
	mem_alloc(&data_2, BLOCK_SIZE);
	mem_alloc(&data_3, BLOCK_SIZE);
	memset(data_1.data, 0xAA, BLOCK_SIZE);
	memset(data_2.data, 0xBB, BLOCK_SIZE);
	memset(data_3.data, 0xCC, BLOCK_SIZE);
	BlockSequence* seq_1 = malloc(sizeof(BlockSequence));
	BlockSequence* seq_2 = malloc(sizeof(BlockSequence));
	BlockSequence* seq_3 = malloc(sizeof(BlockSequence));
	LList* addresses = llist_new();
	addresses->free_element = &free_element_standard;

	seq_1->start_addr = 4;
	seq_2->start_addr = 10;
	seq_3->start_addr = 1000;
	seq_1->length = 1;
	seq_2->length = 1;
	seq_3->length = 1;
	llist_insert(addresses, seq_1);
	llist_insert(addresses, seq_2);
	llist_insert(addresses, seq_3);

	HeapData data = {0};
	mem_alloc(&data, 3 * BLOCK_SIZE);
	memcpy(data.data, data_1.data, BLOCK_SIZE);
	memcpy(&data.data[BLOCK_SIZE], data_2.data, BLOCK_SIZE);
	memcpy(&data.data[BLOCK_SIZE * 2], data_3.data, BLOCK_SIZE);

	fs_write_data_to_disk(&disk, data, *addresses, true);
	HeapData read = fs_read_from_disk(disk, *addresses, true, &ret);
	int cmp = memcmp(data.data, read.data, read.size);
	mu_assert("[MinUnit][TEST] read data from disk: comparison failed", cmp == 0);

	llist_free(addresses);
	mem_free(disk.data_bitmap);
	disk_unmount(disk);
	disk_remove(fname);
	return 0;
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
static char* test_lf_disk_addressing() {
	Disk disk = create_less_fragmented_disk();
	mem_dump(disk.data_bitmap, "LF.bin");
	disk.file = fopen("less_fragmented.bin", "r+");

	const int allocation_size = 266310;
	LList* addresses;
	fs_allocate_blocks(&disk, allocation_size, &addresses);

	Inode inode = {0};
	int ret = stream_write_addresses(&disk, &inode, *addresses);
	LList* all_addresses = stream_read_addresses(disk, inode, &ret);

	bool res = llist_is_equal(*addresses, *all_addresses, &compare_block_sequence);
	mu_assert("[MinUnit][TEST] file disk addressing: not equal pre/post serialization", res);

	// TODO fix  double free
	mem_free(disk.data_bitmap);
	fclose(disk.file);

	return 0;
}

static char* test_lf_disk_addressing_2() {
	Disk disk = create_less_fragmented_disk();
	mem_dump(disk.data_bitmap, "LF.bin");
	disk.file = fopen("less_fragmented.bin", "r+");

	const int allocation_size = 266310;
	LList* addresses;
	fs_allocate_blocks(&disk, allocation_size, &addresses);

	Inode inode = {0};
	int ret = stream_write_addresses(&disk, &inode, *addresses);
	LList* all_addresses = stream_read_addresses(disk, inode, &ret);

	bool res = llist_is_equal(*addresses, *all_addresses, &compare_block_sequence);
	mu_assert("[MinUnit][TEST] file disk addressing less fragmented 1: not equal pre/post serialization", res);

	//llist_free(&all_addresses);
	llist_free(addresses);
	mem_free(disk.data_bitmap);
	fclose(disk.file);

	return 0;
}

static char* test_lf_disk_addressing_3() {
	Disk disk = create_less_fragmented_disk();
	mem_dump(disk.data_bitmap, "LF.bin");
	disk.file = fopen("less_fragmented.bin", "r+");

	const int allocation_size = 31;
	LList* addresses;
	fs_allocate_blocks(&disk, allocation_size, &addresses);

	Inode inode = {0};
	int ret = stream_write_addresses(&disk, &inode, *addresses);
	LList* all_addresses = stream_read_addresses(disk, inode, &ret);

	bool res = llist_is_equal(*addresses, *all_addresses, &compare_block_sequence);
	mu_assert("[MinUnit][TEST] file disk addressing less fragmented 3: not equal pre/post serialization", res);
	
	llist_free(addresses);
	mem_free(disk.data_bitmap);
	fclose(disk.file);

	return 0;
}

static char* test_lf_disk_addressing_4() {
	Disk disk = create_less_fragmented_disk();
	mem_dump(disk.data_bitmap, "LF.bin");
	disk.file = fopen("less_fragmented.bin", "r+");

	const int allocation_size = 64 * 64 * 64;
	LList* addresses;
	fs_allocate_blocks(&disk, allocation_size, &addresses);

	Inode inode = {0};
	int ret = stream_write_addresses(&disk, &inode, *addresses);
	LList* all_addresses = stream_read_addresses(disk, inode, &ret);

	bool res = llist_is_equal(*addresses, *all_addresses, &compare_block_sequence);
	mu_assert("[MinUnit][TEST] file disk addressing less fragmented 4: not equal pre/post serialization", res);
	
	llist_free(addresses);
	mem_free(disk.data_bitmap);
	fclose(disk.file);

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

	LList* all_addresses = stream_read_addresses(disk, inode, &ret);

	bool res = llist_is_equal(*addresses, *all_addresses, &compare_block_sequence);
	mu_assert("[MinUnit][TEST] file disk addressing: not equal pre/post serialization", res);

	// TODO fix this invalid free
	//llist_free(&all_addresses);
	llist_free(addresses);
	mem_free(disk.data_bitmap);
	fclose(disk.file);

	return 0;
}

static char* test_file_disk_addressing_2() {
	Inode inode = {0};
	const int allocation_size = 4;
	int ret = 0;
	Disk disk = create_fragmented_disk();
	disk.file = fopen("fragmented.bin", "r+");


	LList* addresses;
	fs_allocate_blocks(&disk, allocation_size, &addresses);
	
	stream_write_addresses(&disk, &inode, *addresses);
	LList* all_addresses = stream_read_addresses(disk, inode, &ret);

	bool res = llist_is_equal(*addresses, *all_addresses, &compare_block_sequence);
	mu_assert("[MinUnit][TEST] file disk addressing 2: not equal pre/post serialization", res);
	
	llist_free(addresses);
	mem_free(disk.data_bitmap);
	fclose(disk.file);
	return 0;
}

static char* test_file_disk_addressing_3() {
	Inode inode = {0};
	// Partway though the first indirect block
	const int allocation_size = 6 + 46;
	int ret = 0;
	Disk disk = create_fragmented_disk();
	disk.file = fopen("fragmented.bin", "r+");


	LList* addresses;
	fs_allocate_blocks(&disk, allocation_size, &addresses);

	stream_write_addresses(&disk, &inode, *addresses);
	LList* all_addresses = stream_read_addresses(disk, inode, &ret);

	bool res = llist_is_equal(*addresses, *all_addresses, &compare_block_sequence);
	mu_assert("[MinUnit][TEST] file disk addressing 3: not equal pre/post serialization", res);


	llist_free(addresses);
	mem_free(disk.data_bitmap);
	fclose(disk.file);
	return 0;
}

static char* test_file_disk_addressing_4() {
	Inode inode = {0};
	// Partway though the first double block
	const int allocation_size = 6 + 64 + 64 * 38;
	int ret = 0;
	Disk disk = create_fragmented_disk();
	disk.file = fopen("fragmented.bin", "r+");


	LList* addresses;
	fs_allocate_blocks(&disk, allocation_size, &addresses);

	stream_write_addresses(&disk, &inode, *addresses);
	LList* all_addresses = stream_read_addresses(disk, inode, &ret);

	bool res = llist_is_equal(*addresses, *all_addresses, &compare_block_sequence);
	mu_assert("[MinUnit][TEST] file disk addressing 4: not equal pre/post serialization", res);
	
	llist_free(addresses);
	mem_free(disk.data_bitmap);
	fclose(disk.file);
	return 0;
}

static char* test_file_disk_addressing_5() {
	Inode inode = {0};
	// Partway though the first double block
	const int allocation_size = 6 + 64 + 64 * 64 + 32;
	int ret = 0;
	Disk disk = create_fragmented_disk();
	disk.file = fopen("fragmented.bin", "r+");


	LList* addresses;
	fs_allocate_blocks(&disk, allocation_size, &addresses);

	stream_write_addresses(&disk, &inode, *addresses);
	LList* all_addresses = stream_read_addresses(disk, inode, &ret);

	bool res = llist_is_equal(*addresses, *all_addresses, &compare_block_sequence);
	mu_assert("[MinUnit][TEST] file disk addressing 5: not equal pre/post serialization", res);
	
	llist_free(addresses);
	//llist_free(all_addresses);
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
	fs_allocate_blocks(&disk, 128 * 8, &addresses);
	
	BlockSequence* node = addresses->head->element;
	mu_assert("[MinUnit][TEST] alloc blocks continuous: incorrect alloc loaction (1)", node->start_addr == 256 * 8);

	mem_free(block_bitmap);
	return 0;
}

static char* test_alloc_blocks_non_continuous() {
	Superblock superblock;
	fs_create_superblock(&superblock, DISK_SIZE);
	Bitmap block_bitmap = {0};
	mem_alloc(&block_bitmap, superblock.data_block_bitmap_size);
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
	for (int i = 0; i < 10; i++) {
		mem_write_section(&block_bitmap, current_location, filler_4000);
		current_location += 4000 + 1024;
	}

	mem_write_section(&block_bitmap, current_location, filler_5000);
	current_location += 5000 + 512;
	mem_write_section(&block_bitmap, current_location, filler_5000);
	current_location += 5000 + 256;
	
	for (int i = 0; i < 2; i++) {
		mem_write_section(&block_bitmap, current_location, filler_4000);
		current_location += 4000 + 128;
	}

	for (int i = 0; i < 2; i++) {
		mem_write_section(&block_bitmap, current_location, filler_4000);
		current_location += 4000 + 64;
	}

	for (int i = 0; i < 4; i++) {
		mem_write_section(&block_bitmap, current_location, filler_5000);
		current_location += 5000 + 32;
	}

	mem_write_section(&block_bitmap, current_location, filler_10000);
	current_location += 10000 + 200;
	mem_write_section(&block_bitmap, current_location, filler_6000);
	current_location += 6000 + 51;
	mem_write_section(&block_bitmap, current_location, filler_200);
	current_location += 200 + 5;
	mem_write_section(&block_bitmap, current_location, filler_200);
	current_location += 200;
	
	Disk disk = { 0 };
	disk.superblock = superblock;
	disk.data_bitmap = block_bitmap;
	
	LList* addresses;
	disk.superblock.data_bitmap_circular_loc = start_byte;

	fs_allocate_blocks(&disk, 11776, &addresses);

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
	dir_find_next_path_name(path, location, &name);
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
	name.size + 1;	// Add one to get past forward slash	
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

	mem_dump(directory, "dir.bin");

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

	int ret;

	util_string_to_heap("Hello World!", &filename);
	util_string_to_heap("main.c", &filename2);

	entry.name = filename;
	entry.inode_number = 21098740;

	entry2.inode_number = 0x83f7bc82;
	entry2.name = filename2;

	dir_add_entry(&directory, entry);
	ret = memcmp(expected, directory.data, 17);
	mu_assert("[MinUnit][ERROR] directory add file: binary data produced incorrect [1]", ret == 0);


	dir_add_entry(&directory, entry2);
	memcmp(expected2, directory.data, sizeof(expected2));

	mu_assert("[MinUnit][ERROR] directory add file: binary data produced incorrect [2]", ret == 0);
	
	mem_free(directory);
	mem_free(entry.name);
	mem_free(entry2.name);

	return 0;
}

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


	inode.data.indirect.length = 91238234;
	inode.data.indirect.start_addr =  12839782;

	inode.data.indirect.length = 293754239;
	inode.data.indirect.start_addr = 5620234;

	inode.data.indirect.length = 52349843;
	inode.data.indirect.start_addr = 4984234;

	HeapData data;
	mem_alloc(&data, INODE_SIZE);
	serialize_inode(&data, inode);
	unserialize_inode(&data, &inode2);

	int ret = compare_inode(inode, inode2);

	mu_assert("[MinUnit][FAIL] inode serialization: inode changed by serialization", ret == 0);
	mem_free(data);
	return 0;

} 

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
	printf("[....] test_superblock_calculations");
	mu_run_test(test_superblock_calculations);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_superblock_serialization");
	mu_run_test(test_superblock_serialization);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_inode_serialization");
	mu_run_test(test_inode_serialization);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_bitmap_io");
	mu_run_test(test_bitmap_io);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_find_continuous_bitmap_run");
	mu_run_test(test_find_continuous_bitmap_run);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_find_continuous_bitmap_run_2");
	mu_run_test(test_find_continuous_bitmap_run_2);
	printf("\r\e[32m[PASS]\e[0m\n");
	
	printf("[....] test_directory_get_inode_number");
	mu_run_test(test_directory_get_inode_number);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_directory_add_entry");
	mu_run_test(test_directory_add_entry);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_next_dir_name");
	mu_run_test(test_next_dir_name);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_directory_traversal");
	mu_run_test(test_directory_traversal);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_find_next_bitmap_block");
	mu_run_test(test_find_next_bitmap_block);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_api_create_dir");
	mu_run_test(test_api_create_dir);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_dir_read_entry");
	mu_run_test(test_dir_read_entry);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_api_directory_handling");
	mu_run_test(test_api_directory_handling);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_alloc_blocks_continuous");
	mu_run_test(test_alloc_blocks_continuous);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_write_data_to_disk");
	mu_run_test(test_write_data_to_disk);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_write_data_to_disk_2");
	mu_run_test(test_write_data_to_disk_2);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_disk_io");
	mu_run_test(test_disk_io);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_disk_io_2");
	mu_run_test(test_disk_io_2);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_file_disk_addressssing");
	mu_run_test(test_file_disk_addressssing);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_read_from_disk_by_seq");
	mu_run_test(test_read_from_disk_by_seq);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_file_disk_addressing_2");
	mu_run_test(test_file_disk_addressing_2);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_file_disk_addressing_3");
	mu_run_test(test_file_disk_addressing_3);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_file_disk_addressing_4");
	mu_run_test(test_file_disk_addressing_4);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_file_disk_addressing_5");
	mu_run_test(test_file_disk_addressing_5);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_lf_disk_addressing");
	mu_run_test(test_lf_disk_addressing);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_lf_disk_addressing_2");
	mu_run_test(test_lf_disk_addressing_2);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_lf_disk_addressing_3");
	mu_run_test(test_lf_disk_addressing_3);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_lf_disk_addressing_4");
	mu_run_test(test_lf_disk_addressing_4);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_read_from_disk");
	mu_run_test(test_read_from_disk);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_write_inode");
	mu_run_test(test_write_inode);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_read_inode");
	mu_run_test(test_read_inode);
	printf("\r\e[32m[PASS]\e[0m\n");

		printf("[....] test_metedata_load_and_store");
	mu_run_test(test_metedata_load_and_store);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_append_data_to_disk");
	mu_run_test(test_append_data_to_disk);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_fill_unused_allocated_data");
	mu_run_test(test_fill_unused_allocated_data);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_rw_1");
	mu_run_test(test_rw_1);
	printf("\r\e[32m[PASS]\e[0m\n");

	printf("[....] test_div_round_up");
	mu_run_test(test_div_round_up);
	printf("\r\e[32m[PASS]\e[0m\n");

	return 0;
}

void test_run_all(){
	printf("\033[1m[MinUnit] Running All Tests \033[0m\n");
	printf("----------------------------------\n");

    char *result = all_tests();

    printf("----------------------------------\n\n");
   	printf("----------------------------------\n");

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
