/* Observe only the server child created by this launcher invocation. */
#ifndef DARLING_STARTUP_CHILD_H
#define DARLING_STARTUP_CHILD_H

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

static inline bool darlingStartupChildFailed(pid_t child, FILE* errors)
{
	int status;
	pid_t result;
	do {
		result = waitpid(child, &status, WNOHANG);
	} while (result < 0 && errno == EINTR);
	if (result == 0)
		return false;
	if (result < 0) {
		fprintf(errors, "Cannot check darlingserver startup: %s\n", strerror(errno));
	} else if (WIFEXITED(status)) {
		fprintf(errors, "darlingserver exited with status %d before shellspawn became ready\n",
			WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		fprintf(errors, "darlingserver died from signal %d before shellspawn became ready\n",
			WTERMSIG(status));
	} else {
		fprintf(errors, "Unexpected darlingserver startup wait status: %#x\n", status);
	}
	return true;
}

#endif
