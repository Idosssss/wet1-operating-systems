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


/*=============================================================================
* main function
=============================================================================*/
int main(int argc, char* argv[])
{

	setup_signal_handlers();
	while (1) {
		printf("smash > ");
		fflush(stdout);

		if(!fgets(_line, CMD_LENGTH_MAX, stdin)) {
			continue;
		}

		if(strcmp(_line, "\n") == 0) {
			continue;
		}

		CommandResult res = executeCommand(_line);
		if(res == SMASH_QUIT) {
			break;
		}
	}


	return 0;
}

