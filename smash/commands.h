#ifndef COMMANDS_H
#define COMMANDS_H
/*=============================================================================
* includes, defines, usings
=============================================================================*/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

#include "my_system_call.h"

#define CMD_LENGTH_MAX 120
#define ARGS_NUM_MAX 20
#define JOBS_NUM_MAX 100

/*=============================================================================
* error handling - some useful macros and examples of error handling,
* feel free to not use any of this
=============================================================================*/
#define ERROR_EXIT(msg) \
    do { \
        fprintf(stderr, "%s: %d\n%s", __FILE__, __LINE__, msg); \
        exit(1); \
    } while(0);

static inline void* _validatedMalloc(size_t size)
{
    void* ptr = malloc(size);
    if(!ptr) ERROR_EXIT("malloc");
    return ptr;
}

// example usage:
// char* bufffer = MALLOC_VALIDATED(char, MAX_LINE_SIZE);
// which automatically includes error handling
#define MALLOC_VALIDATED(type, size) \
    ((type*)_validatedMalloc((size)))


/*=============================================================================
* error definitions
=============================================================================*/
typedef enum  {
	INVALID_COMMAND = 0,
	//feel free to add more values here or delete this
} ParsingError;

typedef enum {
	SMASH_SUCCESS = 0,
	SMASH_QUIT,
	SMASH_FAIL
	//feel free to add more values here or delete this
} CommandResult;

typedef enum {
    BACKGROUND,
    STOPPED
} JobState;

typedef struct Job {
    int job_id;
    pid_t pid;
    char command[CMD_LENGTH_MAX];
    time_t start_time;
    JobState state;
    struct Job* next;
} Job;

extern Job* job_list;

extern pid_t foreground_pid;
extern char foreground_cmd[CMD_LENGTH_MAX];

/*=============================================================================
* global functions
=============================================================================*/
int parseCommandExample(char* line);

CommandResult executeCommand(char* command);

void cleanFinishedJobs(void);
void printJobs(void);
Job* findJobById(int job_id);
int addJob(pid_t pid, const char* command, JobState state);
void removeJobById(int job_id);

void perrorSmash(const char* command, const char* message);

#endif //COMMANDS_H