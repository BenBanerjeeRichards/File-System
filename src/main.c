#include <stdio.h>
#include <stdlib.h>
#include "disk.h"
#include "constants.h"
#include "memory.h"
#include "fs.h"
#include "serialize.h"
#include "util.h"
#include "test/test.h"
#include "cli.h"

int main(int argc, char** args){
	cli_process_command(args, argc);
	
	//test_run_all();

	return 0;
} 
