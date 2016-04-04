#ifndef CLI_H
#define CLI_H

#include "api.h"

int cli_cmd_newfile(char** arguments, int argument_count);
int cli_cmd_delfile(char** arguments, int argument_count);
int cli_cmd_ls(char** arguments, int argument_count);
int cli_cmd_tofs(char** arguments, int argument_count);

void cli_process_command(char** arguments, int argument_count);

#endif