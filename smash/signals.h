#ifndef SIGNALS_H
#define SIGNALS_H

/*=============================================================================
* includes, defines, usings
=============================================================================*/

#include <sys/types.h>

#define CMD_LENGTH_MAX 120

extern pid_t foreground_pid;
extern char foreground_cmd[CMD_LENGTH_MAX];


/*=============================================================================
* global functions
=============================================================================*/
void setup_signal_handlers(void);
static void sigtstp_handler(int sig);
static void sigint_handler(int sig);



#endif

