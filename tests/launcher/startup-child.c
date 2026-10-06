#define _GNU_SOURCE
#include "../../src/startup/startup-child.h"
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

static void exited(pid_t child)
{
    siginfo_t info;
    assert(waitid(P_PID, child, &info, WEXITED | WNOWAIT) == 0);
}
static void expect_report(pid_t child, const char* expected)
{
    char* message = NULL; size_t length = 0;
    FILE* stream = open_memstream(&message, &length);
    assert(stream);
    assert(darlingStartupChildFailed(child, stream));
    assert(fclose(stream) == 0);
    assert(strstr(message, expected));
    free(message);
}
int main(void)
{
    int pipefd[2]; assert(pipe(pipefd) == 0);
    pid_t live = fork(); assert(live >= 0);
    if (!live) { close(pipefd[1]); char c; (void)read(pipefd[0], &c, 1); _exit(0); }
    close(pipefd[0]);
    FILE* sink = tmpfile(); assert(sink);
    assert(!darlingStartupChildFailed(live, sink));
    assert(ftell(sink) == 0); fclose(sink);

    pid_t unrelated = fork(); assert(unrelated >= 0);
    if (!unrelated) _exit(17);
    exited(unrelated);
    for (int code = 0; code <= 42; code += 42) {
        pid_t child = fork(); assert(child >= 0);
        if (!child) _exit(code);
        exited(child);
        expect_report(child, code ? "exited with status 42" : "exited with status 0");
        int status; assert(waitpid(child, &status, WNOHANG) == -1 && errno == ECHILD);
    }
    assert(kill(live, SIGTERM) == 0); close(pipefd[1]); exited(live);
    expect_report(live, "died from signal 15");
    int status; assert(waitpid(unrelated, &status, 0) == unrelated);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 17);
    expect_report(getpid(), "Cannot check darlingserver startup");
    puts("PASS: live child, exit 0/42, signal, reaping, unrelated child isolation, wait error");
}
