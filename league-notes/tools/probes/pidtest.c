// Does proc_pidinfo see this process and launchd the way launchd needs to (issue #838's probe)?
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <libproc.h>
#include <sys/proc_info.h>
static void probe(const char *who, int pid) {
    struct proc_bsdshortinfo i; memset(&i, 0, sizeof(i)); errno = 0;
    int n = proc_pidinfo(pid, PROC_PIDT_SHORTBSDINFO, 0, &i, sizeof(i));
    printf("%-8s pid=%d proc_pidinfo: n=%d errno=%s comm='%s' ppid=%d uid=%d\n", who, pid, n, n > 0 ? "-" : strerror(errno), i.pbsi_comm, i.pbsi_ppid, i.pbsi_uid);
}
int main(void) { probe("self", getpid()); probe("parent", getppid()); probe("pid 1", 1); return 0; }
