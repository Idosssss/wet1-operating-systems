//commands.c
#include "commands.h"

#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <errno.h>

Job* jobs_list = NULL;
char pwd[CMD_LENGTH_MAX] = "first";

//example function for printing errors from internal commands
void perrorSmash(const char* cmd, const char* msg)
{
	fprintf(stderr, "smash error:%s%s%s\n",
		cmd ? cmd : "",
		cmd ? ": " : "",
		msg);
}

void cleanFinishedJobs(void) {

	Job* curr = jobs_list;
	Job* prev = NULL;

	while (curr != NULL) {
		int status;
		pid_t pid = my_system_call(SYS_WAITPID, curr->pid, &status, WNOHANG);
		if(pid == -1) {
			//waitpid failed
			perrorSmash("waitpid", "waitpid failed");

			prev = curr;
			curr = curr->next;
			continue;
		}
		if(pid == 0) {
			//job not finished
			prev = curr;
			curr = curr->next;
			continue;
		}

		//the job is done, remove
		Job* free = curr;
		if(prev == NULL) {
			//it's the top of the list
			jobs_list = curr->next;
		}else {
			prev->next = curr->next;
		}
		curr = curr->next;
		free(free);
	}
}

void printJobs(void) {
	for(Job* job = jobs_list; job != NULL; job = job->next) {
		time_t now = time(NULL);
		int seconds = (int)difftime(now, job->start_time);

		printf("[%d] %s: %d %d secs", job->job_id, job->command, job->pid, seconds);

		if(job->state == STOPPED) {
			printf(" (STOPPED)");
		}
		printf("\n");
	}
}

Job* findJobById(int job_id) {

	for(Job* curr = jobs_list; curr != NULL; curr = curr->next) {
		if(curr->job_id == job_id) {
			return curr;
		}
	}
	return NULL;
}

int nextJobId(void) {

	if(jobs_list == NULL) {
		return 0;
	}
	int id = 0;
	for(Job* curr = jobs_list; curr != NULL; curr = curr->next) {
		if(curr->job_id != id) {
			return id;
		}
		id++;
	}
	return id;
}

int addJob(pid_t pid, const char* command, JobState state) {

	Job* newJob = MALLOC_VALIDATED(Job, sizeof(Job));
	newJob->job_id = nextJobId();
	newJob->pid = pid;
	strncpy(newJob->command, command, CMD_LENGTH_MAX);
	newJob->start_time = time(NULL);
	newJob->state = state;
	newJob->next = NULL;

	//find its spot in the job list
	if(jobs_list == NULL) {
		jobs_list = newJob;
		return newJob->job_id;
	}
	if(newJob->job_id == 0) {
		newJob->next = jobs_list;
		jobs_list = newJob;
		return newJob->job_id;
	}

	Job* temp = jobs_list;
	for(int i = 0; i < newJob->job_id-1; i++) {
		temp = temp->next;
	}

	newJob->next = temp->next;
	temp->next = newJob;

	return newJob->job_id;
}
void removeJobById(int job_id) {

	Job* curr = jobs_list, *prev = NULL;

	while(curr != NULL) {
		if(curr->job_id == job_id) {
			if(prev == NULL) {
				jobs_list = curr->next;
			}else {
				prev->next = curr->next;
			}
			free(curr);
			return;
		}
		prev = curr;
		curr = curr->next;
	}
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

bool isBuiltin(const char* cmd) {
	return
		strcmp(cmd, "showpid")      == 0 ||
		strcmp(cmd, "pwd")     == 0 ||
		strcmp(cmd, "cd") == 0 ||
		strcmp(cmd, "jobs")    == 0 ||
		strcmp(cmd, "kill")      == 0 ||
		strcmp(cmd, "fg")      == 0 ||
		strcmp(cmd, "bg")    == 0 ||
		strcmp(cmd, "quit")    == 0 ||
		strcmp(cmd, "diff")    == 0 ||
		strcmp(cmd, "alias")   == 0 ||
		strcmp(cmd, "unalias") == 0;

}

bool isNumber(const char* num) {

	for(int i = 0; i < strlen(num); i++) {
		if(!isdigit(num[i])) {
			return false;
		}
	}
	return true;
}

CommandResult cmd_showpid(int argc, char* argv[]) {
	if(argc != 1) {
		perrorSmash("showpid", "expected 0 arguments");
		return SMASH_FAIL;
	}
	pid_t pid = getpid();
	printf("smash pid is %d\n", pid);
	return SMASH_SUCCESS;
}

CommandResult cmd_pwd(int argc, char* argv[]) {
	if(argc != 1) {
		perrorSmash("pwd", "expected 0 arguments");
		return SMASH_FAIL;
	}

	char cwd[CMD_LENGTH_MAX];
	if(!getcwd(cwd, CMD_LENGTH_MAX)) {
		perrorSmash("pwd", "getcwd failed");
		return SMASH_FAIL;
	}
	printf("%s\n", cwd);
	return SMASH_SUCCESS;
}

CommandResult cmd_cd(int argc, char* argv[]) {
	if(argc != 2) {
		perrorSmash("cd", "expected 1 argument");
		return SMASH_FAIL;
	}
	if(strcmp(argv[1], "-") == 0) {
		if(strcmp(pwd, "first") == 0) {
			perrorSmash("cd", "old pwd not set");
			return SMASH_FAIL;
		}
		char temp[CMD_LENGTH_MAX];
		strcpy(temp, pwd);
		getcwd(pwd, CMD_LENGTH_MAX);
		if(chdir(temp) != 0) {
			perrorSmash("cd", "chdir failed");
			return SMASH_FAIL;
		}
		return SMASH_SUCCESS;
	}
	char temp[CMD_LENGTH_MAX];
	strcpy(temp, pwd);
	getcwd(pwd, CMD_LENGTH_MAX);
	if(chdir(argv[1]) != 0) {
		if(errno == ENOTDIR) {
			char buffer[CMD_LENGTH_MAX];
			sprintf(buffer, "%s: not a directory", argv[1]);
			perrorSmash("cd",buffer);
		}else if(errno == ENOENT) {
			perrorSmash("cd", "target directory does not exist");
		}
		strcpy(pwd, temp);
		return SMASH_FAIL;
	}
	return SMASH_SUCCESS;
}

CommandResult cmd_jobs(int argc, char* argv[]) {

	if(argc != 1) {
		perrorSmash("jobs", "expected 0 argument");
		return SMASH_FAIL;
	}

	printJobs();
	return SMASH_SUCCESS;
}

CommandResult cmd_kill(int argc, char* argv[]) {
	if(argc != 3) {
		perrorSmash("kill", "invalid arguments");
		return SMASH_FAIL;
	}
	if(!isNumber(argv[1]) || !isNumber(argv[2])) {
		perrorSmash("kill", "invalid arguments");
		return SMASH_FAIL;
	}

	int sigNum = atoi(argv[1]);
	int jobId = atoi(argv[2]);

	Job* job = findJobById(jobId);
	if(job == NULL) {
		char buffer[CMD_LENGTH_MAX];
		sprintf(buffer, "job id %s does not exist", argv[2]);
		perrorSmash("kill", "job not found");
		return SMASH_FAIL;
	}

	printf("signal %d was sent to pid %d", sigNum, job->pid);
	return SMASH_SUCCESS;
}

CommandResult runBuiltin(int argc, char* argv[])
{
	const char* cmd = argv[0];

	if (strcmp(cmd, "showpid") == 0) return cmd_showpid(argc, argv);
	if (strcmp(cmd, "pwd") == 0) return cmd_pwd(argc, argv);
	if (strcmp(cmd, "cd") == 0) return cmd_cd(argc, argv);
	if (strcmp(cmd, "jobs") == 0) return cmd_jobs(argc, argv);
	if (strcmp(cmd, "kill") == 0) return cmd_kill(argc, argv);
	if (strcmp(cmd, "fg") == 0) return cmd_fg(argc, argv);
	if (strcmp(cmd, "bg") == 0) return cmd_bg(argc, argv);
	if (strcmp(cmd, "quit") == 0) return cmd_quit(argc, argv);
	if (strcmp(cmd, "diff") == 0) return cmd_diff(argc, argv);
	if (strcmp(cmd, "alias") == 0) return cmd_alias(argc, argv);
	if (strcmp(cmd, "unalias") == 0) return cmd_unalias(argc, argv);

	return SMASH_FAIL;
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
