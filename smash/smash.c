//smash.c

/*=============================================================================
* includes, defines, usings
=============================================================================*/
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "commands.h"
#include "signals.h"

/*=============================================================================
* classes/structs declarations
=============================================================================*/

/*=============================================================================
* global variables & data structures
=============================================================================*/
char _line[CMD_LENGTH_MAX];

typedef enum {
	FOREGROUND,
	BACKGROUND,
	STOPPED
} JobState;

typedef struct Job{
	int id;
	pid_t pgid;
	char command[CMD_LENGTH_MAX];
	JobState state;
	struct Job* next;
} Job;



/*=============================================================================
* main function
=============================================================================*/
int main(int argc, char* argv[])
{
	char _cmd[CMD_LENGTH_MAX];
	while(1) {
		printf("smash > ");
		fgets(_line, CMD_LENGTH_MAX, stdin);
		strcpy(_cmd, _line);

		//execute command

		int result = parseCommandExample(_cmd);
		if(result == INVALID_COMMAND) {

		}

		//initialize buffers for next command
		_line[0] = '\0';
		_cmd[0] = '\0';
	}

	return 0;
}

