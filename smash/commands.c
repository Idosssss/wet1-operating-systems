//commands.c
#include "commands.h"

#include <string.h>

Job* jobs_list = NULL;
pid_t foreground_pid = -1;
char foreground_command[CMD_LENGTH_MAX] = {0};

//example function for printing errors from internal commands
void perrorSmash(const char* cmd, const char* msg)
{
	fprintf(stderr, "smash error:%s%s%s\n",
		cmd ? cmd : "",
		cmd ? ": " : "",
		msg);
}

//example function for parsing commands
ParsingError parseCmdExample(char* line, char* argv[ARGS_NUM_MAX+1], int* argc)
{
	char* delimiters = " \t\n"; //parsing should be done by spaces, tabs or newlines
	char* cmd = strtok(line, delimiters); //read strtok documentation - parses string by delimiters
	if(!cmd)
		return INVALID_COMMAND; //this means no tokens were found, most like since command is invalid

	*argc = 1;
	argv[0] = cmd; //first token before spaces/tabs/newlines should be command name
	for(int i = 1; i < ARGS_NUM_MAX; i++)
	{
		argv[i] = strtok(NULL, delimiters); //first arg NULL -> keep tokenizing from previous call
		if(!argv[i])
			break;
		(*argc)++;
	}

	argv[*argc] = NULL;
	return VALID_COMMAND;
}

CommandResult executeCommand(char* cmd) {

	cleanFinishedJobs();

	char* argv[ARGS_NUM_MAX + 1];
	int argc = 0;

	ParsingError error = parseCmdExample(cmd, argv, &argc);
}