// signals.c
#include "signals.h"
#include "smash.c"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


pid_t foreground_pid = -1;
char foreground_cmd[CMD_LENGTH_MAX] = {0};


static void sigint_handler(int sig) {
    (void)sig;

    printf("smash: caught CTRL+C\n");

    if (foreground_pid > 0) {
        if (kill(foreground_pid, SIGKILL) == -1) {
            perror("smash error: kill failed");
            return;
        }

        printf("smash: process %d was killed\n", foreground_pid);
    }
}


static void sigtstp_handler(int sig) {
    (void)sig;

    printf("smash: caught CTRL+Z\n");

    if (foreground_pid > 0) {
        if (kill(foreground_pid, SIGSTOP) == -1) {
            perror("smash error: stop failed");
            return;
        }

        printf("smash: process %d was stopped\n", foreground_pid);

    }
}


void setup_signal_handlers(void) {
    struct sigaction sa_int = {0};
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, NULL);

    struct sigaction sa_tstp = {0};
    sa_tstp.sa_handler = sigtstp_handler;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, NULL);
}

