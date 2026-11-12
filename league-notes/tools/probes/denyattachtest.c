// ptrace(PT_DENY_ATTACH): a program asks the system to refuse debuggers. macOS answers 0, and a later
// attach by another process fails. Also checks that a request number that does not exist is an error
// rather than whatever was left in a register.
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
int main(void) {
	int bad = 0; setvbuf(stdout, 0, _IONBF, 0);
	for (int i = 0; i < 3; i++) {
		errno = 0; int r = ptrace(PT_DENY_ATTACH, 0, 0, 0);
		printf("%s PT_DENY_ATTACH call %d: ret=%d errno=%d\n", r == 0 ? "ok  " : "FAIL", i + 1, r, errno); bad |= r != 0;
	}
	errno = 0; int r = ptrace(99, 0, 0, 0);
	printf("%s request 99 (no such request): ret=%d errno=%d (%s)\n", r == -1 && errno == EINVAL ? "ok  " : "FAIL", r, errno, strerror(errno)); bad |= !(r == -1 && errno == EINVAL);
	pid_t parent = getpid(), c = fork();
	if (c == 0) { errno = 0; int a = ptrace(PT_ATTACHEXC, parent, 0, 0); _exit(a == 0 ? 1 : 0); }   // exit 1: attach was allowed
	int st = 0; waitpid(c, &st, 0);
	int refused = WIFEXITED(st) && WEXITSTATUS(st) == 0;
	printf("%s attach by another process after the denial: %s\n", refused ? "ok  " : "FAIL", refused ? "refused" : "allowed (or the child died)"); bad |= !refused;
	printf(bad ? "RESULT: FAIL\n" : "RESULT: PASS\n"); return bad;
}
