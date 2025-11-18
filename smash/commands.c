//commands.c
#include "commands.h"

#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

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
ParsingError parseCmdExample(char* line, char* argv[ARGS_NUM_MAX+1], int* argc, bool* isBackground)
{
	char* delimiters = " \t\n"; //parsing should be done by spaces, tabs or newlines
	char* cmd = strtok(line, delimiters); //read strtok documentation - parses string by delimiters
	if(!cmd)
		return INVALID_COMMAND; //this means no tokens were found, most like since command is invalid

	*argc = 1;
	*isBackground = false;
	argv[0] = cmd; //first token before spaces/tabs/newlines should be command name
	for(int i = 1; i < ARGS_NUM_MAX; i++)
	{
		argv[i] = strtok(NULL, delimiters); //first arg NULL -> keep tokenizing from previous call
		if(!argv[i])
			break;
		(*argc)++;
	}

	if(strcmp(argv[(*argc)-1], "&") == 0) {
		*isBackground = true;
		(*argc)--;
	}

	argv[*argc] = NULL;
	return VALID_COMMAND;
}

CommandResult executeCommand(char* cmd) {

	//first clean all jobs
	cleanFinishedJobs();

	char original_cmd[CMD_LENGTH_MAX];
	strcpy(original_cmd, cmd);

	//initialize args
	char* argv[ARGS_NUM_MAX + 1];
	int argc = 0;
	bool isBackground = false;

	//parse the command
	ParsingError error = parseCmdExample(cmd, argv, &argc, &isBackground);

	if(error != VALID_COMMAND) {
		perrorSmash(original_cmd, "parsing error");
		return SMASH_FAIL;
	}

	//empty command?
	if(argc == 0) {
		return SMASH_SUCCESS;
	}

	if(isBuiltin(argv[0])) {

		if(isBackground) {

			//background
			const pid_t pid = (pid_t)my_system_call(SYS_FORK);
			if(pid == -1) {
				//fork failed
				perrorSmash(original_cmd, "fork failed");
				return SMASH_FAIL;
			}
			if(pid == 0) {

				//child process
				exit(runBuiltin(argc, argv));
			}

			//parent process
			addJob(pid, original_cmd, BACKGROUND);
			return SMASH_SUCCESS;

		}

		    //foreground
			return runBuiltin(argc, argv);

	}
		//external
	const pid_t pid = (pid_t)my_system_call(SYS_FORK);
	    if(pid == -1) {
		    perrorSmash(original_cmd, "fork failed");
	    	return SMASH_FAIL;
	    }

	if(pid == 0) {
		//child process

		setpgid(0,0);

		my_system_call(SYS_EXECVP, argv[0], argv);
		perrorSmash(original_cmd, "execvp failed");
		exit(EXIT_FAILURE);
	}

	//parent process
	if(isBackground) {
		addJob(pid, original_cmd, BACKGROUND);
		return SMASH_SUCCESS;
	}

	int status;
	if (my_system_call(SYS_WAITPID, pid, &status) == -1) {
		perrorSmash(original_cmd, "waitpid failed");
		return SMASH_FAIL;
	}
	if(WIFSTOPPED(status)) {
		addJob(pid, original_cmd, STOPPED);
		return SMASH_SUCCESS;
	}

	return SMASH_SUCCESS;
}