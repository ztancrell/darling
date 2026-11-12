// fileport_makeport / fileport_makefd: a descriptor wrapped in a Mach port and unwrapped again must
// be the same open file (Chromium's IPC passes descriptors this way).
#include <errno.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int fileport_makeport(int fd, mach_port_t *port);
int fileport_makefd(mach_port_t port);

static int failed;
#define CHECK(cond, what) do { int ok_ = (cond); printf("%s %s\n", ok_ ? "ok  " : "FAIL", what); fflush(stdout); if (!ok_) failed = 1; } while (0)

int main(void) {
	int p[2]; char buf[8] = { 0 };
	pipe(p);
	mach_port_t port = MACH_PORT_NULL;
	int rc = fileport_makeport(p[1], &port);
	if (rc != 0) printf("  errno %d (%s)\n", errno, strerror(errno));
	CHECK(rc == 0 && MACH_PORT_VALID(port), "wrapping a pipe's write end gives a port");
	mach_port_type_t type = 0;
	CHECK(mach_port_type(mach_task_self(), port, &type) == KERN_SUCCESS && (type & MACH_PORT_TYPE_SEND), "and it is a send right");
	int fd = fileport_makefd(port);
	CHECK(fd >= 0 && fd != p[1], "unwrapping gives a new descriptor");
	CHECK(fcntl(fd, F_GETFD) == FD_CLOEXEC, "that is close-on-exec");
	close(p[1]);
	CHECK(write(fd, "hello", 5) == 5 && read(p[0], buf, 5) == 5 && !strcmp(buf, "hello"), "which is the same pipe, still open after the original is closed");
	errno = 0;
	CHECK(fileport_makefd(mach_task_self()) == -1 && errno == EINVAL, "a port that is not a file port is refused");
	CHECK(mach_port_deallocate(mach_task_self(), port) == KERN_SUCCESS, "the port can be released");
	close(fd);
	CHECK(read(p[0], buf, 1) == 0, "and then nothing holds the pipe open (end of file)");
	printf(failed ? "RESULT: FAIL\n" : "RESULT: PASS\n");
	return failed;
}
