// The sandbox really confines file access: after sandbox_init_with_parameters, the kernel refuses
// files outside the profile, in every thread and in child processes, and the process keeps working.
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mach/mach.h>

int sandbox_init_with_parameters(const char *profile, uint64_t flags, const char *const parameters[], char **errorbuf);
int sandbox_check(pid_t pid, const char *operation, int type, ...);

static int failed;
#define CHECK(cond, what) do { int ok_ = (cond); printf("%s %s\n", ok_ ? "ok  " : "FAIL", what); if (!ok_) failed = 1; } while (0)

#define ALLOWED "/tmp/sbtest-allowed/a.txt"
#define DENIED "/tmp/sbtest-denied/b.txt"

static const char profile[] =
	"(version 1)\n(deny default)\n"
	"(allow file-read* (subpath (param \"DIR\")))\n"
	"(allow file-read* (regex #\"^/tmp/sbtest-denied/.*\"))\n"   // cannot be enforced: stays refused
	"(allow mach-lookup (global-name \"com.example.service\"))\n"  // not a file rule
	"(deny file-read* (subpath \"/tmp/sbtest-allowed/none\"))\n";  // cannot be enforced
static const char *const params[] = { "DIR", "/tmp/sbtest-allowed", NULL };

static void put(const char *path, const char *text) {
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0) { write(fd, text, strlen(text)); close(fd); }
}

static int refused(const char *path, int flags) {
	int fd = open(path, flags);
	if (fd >= 0) { close(fd); return 0; }
	return errno == EACCES;
}

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t go = PTHREAD_COND_INITIALIZER;
static int released, thread_refused;
static void *early_thread(void *unused) {
	pthread_mutex_lock(&lock);
	while (!released) pthread_cond_wait(&go, &lock);
	pthread_mutex_unlock(&lock);
	thread_refused = refused(DENIED, O_RDONLY);
	return NULL;
}

int main(void) {
	char *error = NULL;
	char buf[16] = { 0 };
	mach_port_t port = MACH_PORT_NULL;
	pthread_t thread;
	int status = 0;

	mkdir("/tmp/sbtest-allowed", 0755); mkdir("/tmp/sbtest-denied", 0755);
	put(ALLOWED, "allowed"); put(DENIED, "denied");
	CHECK(!refused(ALLOWED, O_RDONLY) && !refused(DENIED, O_RDONLY), "before: both files open");
	CHECK(sandbox_check(getpid(), NULL, 0) == 0, "before: \"am I sandboxed?\" says no");

	// a profile that cannot be read must fail properly and leave the process unconfined (tried in a child)
	pid_t pid = fork();
	if (pid == 0) {
		char *e = NULL;
		int rc = sandbox_init_with_parameters("(version 1) (allow file-read* (subpath", 0, NULL, &e);
		_exit((rc != 0 && e && sandbox_check(getpid(), NULL, 0) == 0 && !refused(DENIED, O_RDONLY)) ? 0 : 1);
	}
	waitpid(pid, &status, 0);
	CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "a broken profile fails with an error message and confines nothing");

	pthread_create(&thread, NULL, early_thread, NULL); // exists before the sandbox is applied

	int rc = sandbox_init_with_parameters(profile, 0, params, &error);
	if (error) printf("  error: %s\n", error);
	CHECK(rc == 0 && error == NULL, "sandbox request succeeds with no error message");
	CHECK(sandbox_check(getpid(), NULL, 0) == 1, "after: \"am I sandboxed?\" says yes");

	int fd = open(ALLOWED, O_RDONLY);
	CHECK(fd >= 0 && read(fd, buf, 7) == 7 && !strcmp(buf, "allowed"), "allowed file still opens and reads");
	CHECK(refused(DENIED, O_RDONLY), "file outside the profile is refused (permission denied)");
	CHECK(refused(ALLOWED, O_WRONLY), "allowed-for-reading file is refused for writing");
	CHECK(refused("/tmp/sbtest-new.txt", O_WRONLY | O_CREAT), "creating a file outside the profile is refused");
	fd = open("/usr/lib/libSystem.B.dylib", O_RDONLY);
	CHECK(fd >= 0, "Darling's own needs: system libraries still readable");

	pthread_mutex_lock(&lock); released = 1; pthread_cond_signal(&go); pthread_mutex_unlock(&lock);
	pthread_join(thread, NULL);
	CHECK(thread_refused, "a thread that existed before the sandbox is confined too");

	pid = fork();
	if (pid == 0) _exit(refused(DENIED, O_RDONLY) && sandbox_check(getpid(), NULL, 0) == 1 ? 0 : 1);
	waitpid(pid, &status, 0);
	CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "a child process inherits the confinement");

	CHECK(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port) == KERN_SUCCESS && malloc(1 << 20) != NULL, "process still talks to darlingserver and allocates memory");

	printf(failed ? "RESULT: FAIL\n" : "RESULT: PASS\n");
	return failed;
}
