// signals.c
#include "signals.h"
#include <signal.h>
#include <stdio.h>
#include "my_system_call.h"
#include "commands.h"

static void sigint_handler(int sig) {
    (void)sig;

    my_system_call(SYS_SIGNAL, SIGINT, sigint_handler);

    printf("smash: caught CTRL+C\n");

    if (foreground_pid > 0) {
        if (my_system_call(SYS_KILL, foreground_pid, SIGKILL) == -1) {
            perrorSmash("kill", "SIGKILL failed");
            return;
        }

        printf("smash: process %d was killed\n", foreground_pid);
    }
}

static void sigtstp_handler(int sig) {
    (void)sig;

    my_system_call(SYS_SIGNAL, SIGTSTP, sigtstp_handler);

    printf("smash: caught CTRL+Z\n");

    if (foreground_pid > 0) {
        if (my_system_call(SYS_KILL, foreground_pid, SIGSTOP) == -1) {
            perrorSmash("kill", "SIGSTOP failed");
            return;
        }

        printf("smash: process %d was stopped\n", foreground_pid);
    }
}

void setup_signal_handlers(void) {
    my_system_call(SYS_SIGNAL, SIGINT, sigint_handler);
    my_system_call(SYS_SIGNAL, SIGTSTP, sigtstp_handler);
}