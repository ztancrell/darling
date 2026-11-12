// Does a child started the way Chromium starts its helpers get the pipe it was promised?
// Parent: pipe -> move read end to a high fd -> posix_spawn(self) with POSIX_SPAWN_CLOEXEC_DEFAULT and
// either addinherit_np(fd) or adddup2(fd, other) -> write a message. Child: read it from the fd named in argv.
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <mach-o/dyld.h>
extern char **environ;

static int run(const char* self, int use_dup2) {
	int p[2]; if (pipe(p)) { perror("pipe"); return 1; }
	int rd = fcntl(p[0], F_DUPFD_CLOEXEC, 134); close(p[0]);
	int target = use_dup2 ? 140 : rd;
	posix_spawnattr_t a; posix_spawn_file_actions_t fa;
	posix_spawnattr_init(&a); posix_spawnattr_setflags(&a, POSIX_SPAWN_CLOEXEC_DEFAULT);
	posix_spawn_file_actions_init(&fa);
	for (int i = 0; i < 3; i++) posix_spawn_file_actions_addinherit_np(&fa, i);
	int rc = use_dup2 ? posix_spawn_file_actions_adddup2(&fa, rd, target) : posix_spawn_file_actions_addinherit_np(&fa, rd);
	char num[16]; snprintf(num, sizeof num, "%d", target);
	char* argv[] = { (char*)self, "child", num, NULL };
	pid_t pid; int err = posix_spawn(&pid, self, &fa, &a, argv, environ);
	if (rc || err) { printf("FAIL %s: file action rc=%d spawn err=%d\n", use_dup2 ? "dup2" : "inherit", rc, err); return 1; }
	close(rd); write(p[1], "hello", 5); close(p[1]);
	int st = 0; waitpid(pid, &st, 0);
	int ok = WIFEXITED(st) && WEXITSTATUS(st) == 0;
	printf("%s %s: child fd %d\n", ok ? "ok  " : "FAIL", use_dup2 ? "dup2   " : "inherit", target);
	return !ok;
}

int main(int argc, char** argv) {
	if (argc == 3 && !strcmp(argv[1], "child")) {
		char buf[8] = {0}; int fd = atoi(argv[2]); ssize_t n = read(fd, buf, 5);
		if (n != 5 || strcmp(buf, "hello")) { printf("  child: read(%d) = %zd errno=%d (%s)\n", fd, n, n < 0 ? errno : 0, n < 0 ? strerror(errno) : ""); return 1; }
		return 0;
	}
	char self[4096]; uint32_t len = sizeof self; _NSGetExecutablePath(self, &len);
	int f = run(self, 0) | run(self, 1);
	printf(f ? "RESULT: FAIL\n" : "RESULT: PASS\n");
	return f;
}
