//commands.c
#include "commands.h"
#include "signal.h"

#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <errno.h>

#include <sys/stat.h>

Job* jobs_list = NULL;
pid_t foreground_pid = -1;
char foreground_cmd[CMD_LENGTH_MAX] = {0};
char pwd[CMD_LENGTH_MAX] = "first";

Alias* alias_list = NULL;

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
		Job* freeJob = curr;
		if(prev == NULL) {
			//it's the top of the list
			jobs_list = curr->next;
		}else {
			prev->next = curr->next;
		}
		curr = curr->next;
		free(freeJob);
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
	strncpy(newJob->command, command, CMD_LENGTH_MAX - 1);
	newJob->command[CMD_LENGTH_MAX - 1] = '\0';
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

Job* findMaxIdJobForFG(void) {
	Job* max = NULL;
	for(Job* curr = jobs_list; curr != NULL; curr = curr->next) {
		if(!max || curr->job_id > max->job_id) {
			max = curr;
		}
	}
	return max;
}

Job* findMaxIdJobForBG(void) {
	Job* max = NULL;
	for(Job* curr = jobs_list; curr != NULL; curr = curr->next) {
		if((!max || curr->job_id > max->job_id) && curr->state == STOPPED) {
			max = curr;
		}
	}
	return max;
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
    cleanFinishedJobs();
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

	const int sigNum = atoi(argv[1]);
	const int jobId = atoi(argv[2]);

	Job* job = findJobById(jobId);
	if(job == NULL) {
		char buffer[CMD_LENGTH_MAX];
		sprintf(buffer, "job id %s does not exist", argv[2]);
		perrorSmash("kill", buffer);
		return SMASH_FAIL;
	}

	printf("signal %d was sent to pid %d", sigNum, job->pid);
	return SMASH_SUCCESS;
}

CommandResult cmd_fg(int argc, char* argv[]) {

	if(argc != 1 && argc != 2) {
		perrorSmash("fg", "invalid arguments");
		return SMASH_FAIL;
	}

	Job* job = NULL;

	if(argc == 1) {
		job = findMaxIdJobForFG();
		if(job == NULL) {
			perrorSmash("fg", "job list is empty");
			return SMASH_FAIL;
		}
	} else {
		if(!isNumber(argv[1])) {
			perrorSmash("fg", "invalid arguments");
			return SMASH_FAIL;
		}

		int job_id = atoi(argv[1]);
		job = findJobById(job_id);

		if(job == NULL) {
			char buffer[CMD_LENGTH_MAX];
			sprintf(buffer, "job id %s does not exist", argv[1]);
			perrorSmash("fg", buffer);
			return SMASH_FAIL;
		}
	}

	printf("[%d] %s\n", job->job_id, job->command);

	if(job->state == STOPPED) {
		my_system_call(SYS_KILL, job->pid, SIGCONT);
	}

	foreground_pid = job->pid;
	strncpy(foreground_cmd, job->command, CMD_LENGTH_MAX-1);
	foreground_cmd[CMD_LENGTH_MAX-1] = '\0';

	removeJobById(job->job_id);

	int status;
	if(my_system_call(SYS_WAITPID, job->pid, &status, 0) == -1) {
		perrorSmash("fg", "waitpid failed");
		return SMASH_FAIL;
	}

	foreground_pid = -1;
	foreground_cmd[0] = '\0';
	return SMASH_SUCCESS;

}

CommandResult cmd_bg(int argc, char* argv[]) {

	if(argc != 2 && argc != 1) {
		perrorSmash("bg", "invalid arguments");
		return SMASH_FAIL;
	}

	Job* job = NULL;
	if(argc == 1) {
		job = findMaxIdJobForBG();
		if(job == NULL) {
			perrorSmash("bg", "there are no topped jobs to resume");
			return SMASH_FAIL;
		}
	}else {
		if(!isNumber(argv[1])) {
			perrorSmash("bg", "invalid arguments");
			return SMASH_FAIL;
		}
		int job_id = atoi(argv[1]);
		job = findJobById(job_id);

		if(job == NULL) {
			char buffer[CMD_LENGTH_MAX];
			sprintf(buffer, "job id %s does not exist", argv[1]);
			perrorSmash("bg", buffer);
			return SMASH_FAIL;
		}
		if(job->state != STOPPED) {
			char buffer[CMD_LENGTH_MAX];
			sprintf(buffer, "job id %s is already in background", argv[1]);
			perrorSmash("bg", buffer);
			return SMASH_FAIL;
		}
	}

	printf("[%d] %s\n", job->job_id, job->command);
	job->state = BACKGROUND;
	my_system_call(SYS_KILL, job->pid, SIGCONT);
	return SMASH_SUCCESS;
}

CommandResult cmd_quit(int argc, char* argv[]) {

	if(argc != 1 && argc != 2) {
		perrorSmash("quit", "expected 0 or 1 arguments");
		return SMASH_FAIL;
	}

	if(argc == 1) {
		return SMASH_QUIT;
	}

	if(strcmp(argv[1], "kill") != 0) {
		perrorSmash("quit", "unexpected arguments");
		return SMASH_FAIL;
	}

	int status;
	Job* job = jobs_list;
	Job* to_free = NULL;
	while(job != NULL) {

		printf("[%d] %s - ", job->job_id, job->command);
		my_system_call(SYS_KILL, job->pid, SIGTERM);
		printf("sending SIGTERM... ");
		sleep(5);
		if(my_system_call(SYS_WAITPID, job->pid, &status, WNOHANG) == 0) {
			my_system_call(SYS_KILL, job->pid, SIGKILL);
			printf("sending SIGKILL... done\n");
		}else {
			printf("done\n");
		}

		to_free = job;
		job = job->next;
		free(to_free);
	}

	return SMASH_QUIT;
}

static bool pathExists(char* path) {
	struct stat st;
	return stat(path, &st) == 0;
}

static bool isFile(char* path) {
	struct stat st;
	if(stat(path, &st) != 0) {
		return false;
	}
	return S_ISREG(st.st_mode);
}

static bool areFilesEqual(char* path1, char* path2) {

	FILE *f1 = fopen(path1, "rb");
	FILE *f2 = fopen(path2, "rb");

	if(f1 == NULL || f2 == NULL) {
		if(f1) {
			fclose(f1);
		}
		if(f2) {
			fclose(f2);
		}
		return false;
	}

	int c1, c2;
	do {
		c1 = fgetc(f1);
		c2 = fgetc(f2);
		if(c1 != c2) {
			fclose(f1);
			fclose(f2);
			return false;
		}
	}while (c1 != EOF && c2 != EOF);

	fclose(f1);
	fclose(f2);
	return true;
}

CommandResult cmd_diff(int argc, char* argv[]) {

	if(argc != 3) {
		perrorSmash("diff", "expected 2 arguments");
		return SMASH_FAIL;
	}
	if(!pathExists(argv[1]) || !pathExists(argv[2])) {
		perrorSmash("diff", "expected valid paths for files");
		return SMASH_FAIL;
	}

	if(!isFile(argv[1]) || !isFile(argv[2])) {
		perrorSmash("diff", "paths are not files");
		return SMASH_FAIL;
	}

	if(areFilesEqual(argv[1], argv[2])) {
		printf("1\n");
		return SMASH_SUCCESS;
	}
	printf("0\n");
	return SMASH_SUCCESS;
}


Alias* findAlias(char* name) {
    Alias* curr = alias_list;
    while(curr) {
        // return current if found
        if(strcmp(curr->alias, name) == 0) return curr;
        curr = curr->next;
    }
    //didnt find the alias in the linked list
    return NULL;
}

void addAlias(char* name, char* command) {
    //checks for alias
    Alias* found = findAlias(name);
    if(found) {

        //found so puts the command in the alias
        strcpy(found->command, command);
    } else {
        //didnt find the alias so created a new ALIAS
        Alias* new_alias = MALLOC_VALIDATED(Alias, sizeof(Alias));
        strcpy(new_alias->alias, name);
        strcpy(new_alias->command, command);
        new_alias->next = alias_list;
        alias_list = new_alias;
    }
}

void removeAlias(char* name) {
    Alias* curr = alias_list;
    Alias* prev = NULL;
    while(curr) {
        if(strcmp(curr->alias, name) == 0) {
            if(prev == NULL) alias_list = curr->next;
            else prev->next = curr->next;
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }

    //looped through the entire thing and didnt find the alias
    char error_msg[CMD_LENGTH_MAX];
    sprintf(error_msg, "alias %s does not exist", name);
    perrorSmash("unalias", error_msg);
}

CommandResult cmd_alias(int argc, char* argv[]) {
    if (argc == 1) {
        Alias* curr = alias_list;
        while(curr) {
            printf("%s='%s'\n", curr->alias, curr->command);
            curr = curr->next;
        }
        return SMASH_SUCCESS;
    }

    char cmd_line[CMD_LENGTH_MAX] = {0};
    for (int i = 1; i < argc; i++) {
        if (i > 1) strcat(cmd_line, " ");
        strcat(cmd_line, argv[i]);
    }

    char* sep = strchr(cmd_line, '=');
    if (!sep) {
        perrorSmash("alias", "invalid alias format");
        return SMASH_FAIL;
    }

    *sep = '\0';
    char* name = cmd_line;
    char* command = sep + 1;

    if (command[0] == '\'' || command[0] == '\"') {
        char quote_type = command[0];
        command++;
        char* end = strrchr(command, quote_type);
        if (end) {
            *end = '\0';
        } else {
            perrorSmash("alias", "invalid alias format");
            return SMASH_FAIL;
        }
    }

    if (isBuiltin(name)) {
        perrorSmash("alias", "alias name already exists");
        return SMASH_FAIL;
    }

    addAlias(name, command);
    return SMASH_SUCCESS;
}

CommandResult cmd_unalias(int argc, char* argv[]) {
    if (argc != 2) {
        perrorSmash("unalias", "expected 1 argument");
        return SMASH_FAIL;
    }
    removeAlias(argv[1]);
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
CommandResult executeSingleCommand(char* cmd) {

    char original_cmd[CMD_LENGTH_MAX];
    strcpy(original_cmd, cmd);

    char* argv[ARGS_NUM_MAX + 1];
    int argc = 0;
    bool isBackground = false;

    ParsingError error = parseCmdExample(cmd, argv, &argc, &isBackground);

    if(error != VALID_COMMAND) {
        perrorSmash(original_cmd, "parsing error");
        return SMASH_FAIL;
    }

    if(argc == 0) {
        return SMASH_SUCCESS;
    }

    if(isBuiltin(argv[0])) {
        if(isBackground) {
            const pid_t pid = (pid_t)my_system_call(SYS_FORK);
            if(pid == -1) {
                perrorSmash(original_cmd, "fork failed");
                return SMASH_FAIL;
            }
            if(pid == 0) {
                setpgid(0, 0);
                exit(runBuiltin(argc, argv));
            }
            addJob(pid, original_cmd, BACKGROUND);
            return SMASH_SUCCESS;
        }
        return runBuiltin(argc, argv);
    }

    const pid_t pid = (pid_t)my_system_call(SYS_FORK);
    if(pid == -1) {
        perrorSmash(original_cmd, "fork failed");
        return SMASH_FAIL;
    }

    if(pid == 0) {
        setpgid(0, 0);
        my_system_call(SYS_EXECVP, argv[0], argv);
        perrorSmash(original_cmd, "execvp failed");
        exit(EXIT_FAILURE);
    }

    if(isBackground) {
        addJob(pid, original_cmd, BACKGROUND);
        return SMASH_SUCCESS;
    }

    foreground_pid = pid;
    strncpy(foreground_cmd, original_cmd, CMD_LENGTH_MAX - 1);
    foreground_cmd[CMD_LENGTH_MAX - 1] = '\0';

    int status;
    pid_t wait_result;

    do {
        wait_result = my_system_call(SYS_WAITPID, pid, &status, WUNTRACED);
    } while (wait_result == -1 && errno == EINTR);

    if (wait_result == -1) {
        perrorSmash(original_cmd, "waitpid failed");
        foreground_pid = -1;
        foreground_cmd[0] = '\0';
        return SMASH_FAIL;
    }

    foreground_pid = -1;
    foreground_cmd[0] = '\0';

    if(WIFSTOPPED(status)) {
        addJob(pid, original_cmd, STOPPED);
        return SMASH_SUCCESS;
    }

    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) != 0) {
            return SMASH_FAIL;
        }
    }

    if (WIFSIGNALED(status)) {
        return SMASH_FAIL;
    }

    return SMASH_SUCCESS;
}


CommandResult executeCommand(char* cmd_line) {
    cleanFinishedJobs();

    char cmd_for_alias[CMD_LENGTH_MAX];
    strncpy(cmd_for_alias, cmd_line, CMD_LENGTH_MAX);
    cmd_for_alias[CMD_LENGTH_MAX-1] = '\0';

    char* first_word = strtok(cmd_for_alias, " \t\n");

    if (first_word) {
        Alias* alias = findAlias(first_word);
        if (alias) {
            char* args = cmd_line;
            while(*args && (*args == ' ' || *args == '\t' || *args == '\n')) args++;
            args += strlen(first_word);

            char new_cmd[CMD_LENGTH_MAX];
            sprintf(new_cmd, "%s%s", alias->command, args);
            return executeCommand(new_cmd);
        }
    }

    char cmd_copy[CMD_LENGTH_MAX];
    strncpy(cmd_copy, cmd_line, CMD_LENGTH_MAX);
    cmd_copy[CMD_LENGTH_MAX-1] = '\0';

    char* left = cmd_copy;
    char* p = left;
    bool in_single_quote = false;
    bool in_double_quote = false;
    CommandResult res = SMASH_SUCCESS;

    while (*p) {
        if (*p == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        } else if (*p == '\"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        }

        if (!in_single_quote && !in_double_quote && strncmp(p, "&&", 2) == 0) {
            *p = '\0';

            res = executeSingleCommand(left);

            if (res == SMASH_FAIL) return SMASH_FAIL;
            if (res == SMASH_QUIT) return SMASH_QUIT;

            left = p + 2;
            while(*left == ' ') left++;

            p = left;
            continue;
        }
        p++;
    }

    return executeSingleCommand(left);
}

