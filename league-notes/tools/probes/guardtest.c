// change_fdguard_np: adding a guard to an open descriptor must succeed (Chromium treats failure as fatal).
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef uint64_t guardid_t;
int change_fdguard_np(int fd, const guardid_t *guard, unsigned guardflags, const guardid_t *nguard, unsigned nguardflags, int *fdflagsp);
int guarded_close_np(int fd, const guardid_t *guard);

static int failed;
#define CHECK(cond, what) do { int ok_ = (cond); printf("%s %s\n", ok_ ? "ok  " : "FAIL", what); if (!ok_) failed = 1; } while (0)

int main(void) {
	guardid_t guard = 0x1234abcd;
	int flags = -1;
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	fcntl(fd, F_SETFD, FD_CLOEXEC);

	int rc = change_fdguard_np(fd, NULL, 0, &guard, 1 /* GUARD_CLOSE */ | 2 /* GUARD_DUP */, &flags);
	if (rc != 0) printf("  errno %d (%s)\n", errno, strerror(errno));
	CHECK(rc == 0, "adding a guard to an open socket succeeds");
	CHECK(flags == FD_CLOEXEC, "the descriptor's flags are reported back");
	CHECK(guard == 0x1234abcd, "the caller's guard value is left alone");
	errno = 0;
	CHECK(change_fdguard_np(9999, NULL, 0, &guard, 1, NULL) == -1 && errno == EBADF, "a descriptor that is not open gives \"bad file descriptor\"");
	CHECK(guarded_close_np(fd, &guard) == 0, "guarded close of it succeeds");
	CHECK(fcntl(fd, F_GETFD) == -1, "and the descriptor is really closed");
	// macOS takes the guard as const; Chromium passes one that lives in read-only memory
	static const guardid_t fixed = 0x5678;
	int fd2 = socket(AF_INET, SOCK_DGRAM, 0);
	CHECK(guarded_close_np(fd2, &fixed) == 0, "guarded close with a guard in read-only memory succeeds");
	printf(failed ? "RESULT: FAIL\n" : "RESULT: PASS\n");
	return failed;
}
