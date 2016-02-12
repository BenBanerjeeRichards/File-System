#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../fs.h"
#include "../util.h"
#include "../memory.h"
#include "../serialize.h"
#include "test.h"

int tests_run = 0;

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
	int t = util_path_next_dir_name(path, location, &name);
	location += name.size + 1;	// Add one to get past forward slash	
	int ret = memcmp(name.data, expected1, name.size);
	mu_assert("[MinUnit][TEST] next dir name: dir name read is incorrect [1]", ret == 0);
	mu_assert("[MinUnit][TEST] next dir name: read blank dir name [1]", name.size != 0);
	mu_assert("[MinUnit][TEST] next dir name: length of directory incorrect [1]", name.size == 4);
	mem_free(name);

	util_path_next_dir_name(path, location, &name);
	location += name.size + 1;	// Add one to get past forward slash	
	ret = memcmp(name.data, expected2, name.size);
	mu_assert("[MinUnit][TEST] next dir name: dir name read is incorrect [2]", ret == 0);
	mu_assert("[MinUnit][TEST] next dir name: read blank dir name [2]", name.size != 0);
	mu_assert("[MinUnit][TEST] next dir name: length of directory incorrect [1]", name.size == 10);
	mem_free(name);
	
	util_path_next_dir_name(path, location, &name);
	location += name.size + 1;	// Add one to get past forward slash	
	ret = memcmp(name.data, expected3, name.size);
	mu_assert("[MinUnit][TEST] next dir name: dir name read is incorrect [3]", ret == 0);
	mu_assert("[MinUnit][TEST] next dir name: read blank dir name [3]", name.size != 0);
	mu_assert("[MinUnit][TEST] next dir name: length of directory incorrect [1]", name.size == 7);
	mem_free(name);

	util_path_next_dir_name(path, location, &name);
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
	//uint8_t bitmap_data[] = {0xFE, 0x00, 0x0F, 0xFE, 0xCC, 0x00, 0x06, 0xEE, 0x34, 0x8F};
	uint8_t bitmap_data[] = {0xFE, 0x00, 0xFF, 0xFE, 0xCC, 0x00, 0x06, 0xEE, 0x34, 0x8F};
	
	for (int i = 0; i < 10; i++) {
		mem_write(&bitmap, i, bitmap_data[i]);
	}
	
	int start = 0;
	fs_find_continuous_bitmap_run(bitmap, length, start_byte, &start);
	
	mu_assert("[MinUnit][TEST] find bitmap run: incorrect start bit", start == 37);
	
	int ret = fs_find_continuous_bitmap_run(bitmap, 17, 3, &start);
	mu_assert("[MinUnit][TEST} find bitmap run: found bitmap run, but expected error", ret == ERR_NO_BITMAP_RUN_FOUND);

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

	fs_add_directory_entry(&directory, entry1);
	fs_add_directory_entry(&directory, entry2);
	fs_add_directory_entry(&directory, entry3);
	fs_add_directory_entry(&directory, entry4);
	fs_add_directory_entry(&directory, entry5);
	fs_add_directory_entry(&directory, entry6);

	uint32_t inode_number = 0;
	fs_directory_get_inode_number(directory, entry1.name, &inode_number);
	mu_assert("[MinUnit][FAIL] directory get inode: retrieved inode incorrect [1]", inode_number == 988722354);
	fs_directory_get_inode_number(directory, entry2.name, &inode_number);
	mu_assert("[MinUnit][FAIL] directory get inode: retrieved inode incorrect [2]", inode_number == 673829463);
	fs_directory_get_inode_number(directory, entry3.name, &inode_number);
	mu_assert("[MinUnit][FAIL] directory get inode: retrieved inode incorrect [3]", inode_number == 382647549);
	fs_directory_get_inode_number(directory, entry4.name, &inode_number);
	mu_assert("[MinUnit][FAIL] directory get inode: retrieved inode incorrect [4]", inode_number == 769823473);
	fs_directory_get_inode_number(directory, entry5.name, &inode_number);
	mu_assert("[MinUnit][FAIL] directory get inode: retrieved inode incorrect [5]", inode_number == 102938743);
	fs_directory_get_inode_number(directory, entry6.name, &inode_number);
	mu_assert("[MinUnit][FAIL] directory get inode: retrieved inode incorrect [6]", inode_number == 587493523);
	int ret = fs_directory_get_inode_number(directory, invalid, &inode_number);
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

	fs_add_directory_entry(&directory, entry);
	ret = memcmp(expected, directory.data, 17);
	mu_assert("[MinUnit][ERROR] directory add file: binary data produced incorrect [1]", ret == 0);


	ret = fs_add_directory_entry(&directory, entry2);
	ret = memcmp(expected2, directory.data, sizeof(expected2));

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

}

static char* test_superblock_serialization() {
	Superblock superblock = {0};
	Superblock superblock2 = {0};

	fs_create_superblock(&superblock, 4096);

	// Manually set some values which would just be zero otherwise
	superblock.num_used_blocks = 523453;
	superblock.num_used_inodes = 2323;
	superblock.data_bitmap_circular_loc = 98734223;
	superblock.flags = 0xAABB;	
	HeapData data = {0};
	mem_alloc(&data, 512);

	serialize_superblock(&data, superblock);
	unserialize_superblock(&data, &superblock2);

	mem_free(data);
	int ret = compare_superblock(superblock, superblock2);
	mu_assert("[MinUnit][FAIL] superblock serialization: superblock changed by serialization", ret == 0);
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
	mu_assert("[MinUnit][FAIL] superblock calculation: data block bitmap size incorrect", superblock.data_block_bitmap_size == 458240);
	mu_assert("[MinUnit][FAIL] superblock calculation: inode table start address incorrect", superblock.inode_table_start_addr == 2);
	mu_assert("[MinUnit][FAIL] superblock calculation: data block bitmap addr incorrect", superblock.data_block_bitmap_addr == 130);
	mu_assert("[MinUnit][FAIL] superblock calculation: data block start addr incorrect", superblock.data_blocks_start_addr == 1025);

	return 0;
}

static char* test_bitmap_io() {
	HeapData block = {0};
	int ret = mem_alloc(&block, 8);
	mu_assert("[MinUnit][FAIL] bitmap io: failed to alloc block data", ret == SUCCESS);

	ret = 0;
	ret += fs_write_bitmap_bit(&block, 0, 1);
	ret += fs_write_bitmap_bit(&block, 1, 0);
	ret += fs_write_bitmap_bit(&block, 2, 0);
	ret += fs_write_bitmap_bit(&block, 3, 1);
	ret += fs_write_bitmap_bit(&block, 4, 0);
	ret += fs_write_bitmap_bit(&block, 5, 1);
	ret += fs_write_bitmap_bit(&block, 6, 1);
	ret += fs_write_bitmap_bit(&block, 7, 0);
	ret += fs_write_bitmap_bit(&block, 8, 1);
	ret += fs_write_bitmap_bit(&block, 9, 32);
	ret += fs_write_bitmap_bit(&block, 10, 0);
	ret += fs_write_bitmap_bit(&block, 11, 1);
	ret += fs_write_bitmap_bit(&block, 12, 1);
	ret += fs_write_bitmap_bit(&block, 13, 1);
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
		int v = fs_read_bitmap_bit(block, i, &ret);
		
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
	mu_run_test(test_superblock_serialization);
	mu_run_test(test_superblock_calculations);
	mu_run_test(test_bitmap_io);
	mu_run_test(test_inode_serialization);
	mu_run_test(test_directory_get_inode_number);
	mu_run_test(test_directory_add_entry);
	mu_run_test(test_find_continuous_bitmap_run);
	mu_run_test(test_next_dir_name);
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
