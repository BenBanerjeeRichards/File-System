#include "cli.h"
#include "test/test.h"

void show_usage() {
	const char* usage = "USAGE: fs <command> <arguments>\n"
		"fs: A file system in a file (tm)\n\n"
		"fs newfile <path> \t\t Create a new file at <path>\n"
		"fs delfile <path> \t\t Delete the file at <path>\n"
		"fs ls <path> \t\t\t List all of the files at the directory <path>\n"
		"fs tofs <syspath> <path> \t Move a file on the computer's filesystem <syspath> to this file system at location <path>";

	printf("%s\n",usage);
}

void cli_process_command(char** arguments, int argument_count) {
	if(argument_count < 3) {
		show_usage();
		return;
	}

	if(strcmp("newfile", arguments[1]) ==  0) {
		cli_cmd_newfile(arguments, argument_count);
	} else if(strcmp("delfile", arguments[1]) == 0) {
		cli_cmd_delfile(arguments, argument_count);
	} else if(strcmp("ls", arguments[1]) == 0) {
		cli_cmd_ls(arguments, argument_count);
	} else if(strcmp("tofs", arguments[1]) == 0) {
		cli_cmd_tofs(arguments, argument_count);
	} else if (strcmp("test", arguments[1]) == 0) {
		test_run_all();
		return;
	} else {
		show_usage();
		return;
	}

}

int	cli_cmd_newfile(char** arguments, int argument_count) {
	printf("newfile\n");

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