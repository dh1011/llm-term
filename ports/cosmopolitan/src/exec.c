#define _POSIX_C_SOURCE 200809L

#include "llm_term.h"

#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

extern char **environ;

int llmt_execute_command(const LlmtShell *shell, const char *command, LlmtError *err) {
    pid_t pid;
    int status;
    char *const argv[] = {(char *)shell->command, (char *)shell->arg, (char *)command, NULL};
    int rc = posix_spawnp(&pid, shell->command, NULL, NULL, argv, environ);
    if (rc != 0) {
        llmt_error(err, "failed to start %s: %s", shell->command, strerror(rc));
        return 0;
    }
    if (waitpid(pid, &status, 0) < 0) {
        llmt_error(err, "failed to wait for command: %s", strerror(errno));
        return 0;
    }
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code != 0) {
            llmt_error(err, "command exited with status %d", code);
            return 0;
        }
        return 1;
    }
    if (WIFSIGNALED(status)) {
        llmt_error(err, "command terminated by signal %d", WTERMSIG(status));
        return 0;
    }
    llmt_error(err, "command ended unexpectedly");
    return 0;
}
