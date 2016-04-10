#include "cli.h"
#include "util.h"
#include "test/test.h"
#include "api.h"
#include "constants.h"

void show_usage() {
	const char* usage = "USAGE: fs <command> <arguments>\n"
		"fs: A file system in a file (tm)\n\n"
		"fs newfile <path> \t\t Create a new file at <path>\n"
		"fs delfile <path> \t\t Delete the file at <path>\n"
		"fs ls <path> \t\t\t List all of the files at the directory <path>\n"
		"fs tofs <syspath> <path> \t Move a file on the computer's filesystem <syspath> to this file system at location <path>";
		"fs new \t\t Create a new filesystem";

	printf("%s\n",usage);
}

void cli_process_command(char** arguments, int argument_count) {
	if(argument_count < 2) {
		show_usage();
		return;
	}

	if(strcmp("newfile", arguments[1]) == 0) {
		cli_cmd_newfile(arguments, argument_count);
	} else if (strcmp("new", arguments[1]) == 0) {
		cli_cmd_new(arguments, argument_count);
	} else if(strcmp("delfile", arguments[1]) == 0) {
		cli_cmd_delfile(arguments, argument_count);
	} else if(strcmp("ls", arguments[1]) == 0) {
		cli_cmd_ls(arguments, argument_count);
	} else if(strcmp("tofs", arguments[1]) == 0) {
		cli_cmd_tofs(arguments, argument_count);
	}  else if(strcmp("newdir", arguments[1]) == 0) {
		cli_cmd_newdir(arguments, argument_count);
	} else if (strcmp("test", arguments[1]) == 0) {
		test_run_all();
		return;
	} else {
		show_usage();
		return;
	}

}

int cli_cmd_new(char** arguments, int argument_count) {
	int ret = 0;
	
	Disk disk = fs_create_filesystem(FILESYSTEM_FILE_NAME, DISK_SIZE, &ret);
	if(ret != SUCCESS) return ret;

	ret = fs_write_metadata(disk);
	if(ret != SUCCESS) return ret;

	return SUCCESS;
}

Disk cli_get_disk(int* error) {
	int ret;
	Disk disk = {0};
	disk.file = fopen(FILESYSTEM_FILE_NAME, "r+");
	disk.size = DISK_SIZE;

	if(disk.file == NULL) {
		printf("Error: No file system found (%s). Use <fs new> to create one.\n", FILESYSTEM_FILE_NAME);
		*error =  ERR_DISK_NOT_MOUNTED;
		return disk;
	}

	ret = fs_read_metadata(&disk);
	if(ret != SUCCESS) {
		printf("Error: Failed to read metadata from disk\n");
		*error = ret;
		return disk;
	}

	return disk;
}

int	cli_cmd_newfile(char** arguments, int argument_count) {
	if(argument_count != 3) {
		show_usage();
		return -1;
	}
	
	int ret = 0;
	Disk disk = cli_get_disk(&ret);
	if(ret != SUCCESS) return ret;

	HeapData path = {0};
	Permissions perm = {0};
	util_string_to_heap(arguments[2], &path);

	ret = api_create_file(disk, perm, path);
	if(ret != SUCCESS) {
		printf("Error: Failed to create file, error code %i\n", ret);
		return ret;
	}

	fs_write_metadata(disk);


	return SUCCESS;
}

int	cli_cmd_delfile(char** arguments, int argument_count) {
	printf("delfile\n");

	return SUCCESS;
}

int	cli_cmd_ls(char** arguments, int argument_count) {
	printf("ls\n");

	return SUCCESS;
}

int	cli_cmd_tofs(char** arguments, int argument_count) {
	printf("tofs\n");

	return SUCCESS;
}

int	cli_cmd_newdir(char** arguments, int argument_count) {
	if(argument_count != 3 && argument_count != 4) {
		show_usage();
		return -1;
	}

	int ret = 0;
	Disk disk = cli_get_disk(&ret);
	if(ret != SUCCESS) return ret;

	printf("%i\n", disk.superblock.data_blocks_start_addr);

	HeapData path = {0};
	HeapData name = {0};

	if(argument_count == 3) {
		util_string_to_heap("", &path);
		util_string_to_heap(arguments[2], &name);
	} else {
		util_string_to_heap(arguments[2], &path);
		util_string_to_heap(arguments[3], &name);
	}

	ret = api_create_dir(&disk, path, name);
	if(ret != SUCCESS) {
		printf("Error: Failed to create dir, error code %i\n", ret);
		return ret;
	}

	fs_write_metadata(disk);
	return SUCCESS;
}