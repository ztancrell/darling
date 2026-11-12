// proc_pidfdinfo(PROC_PIDFDVNODEPATHINFO): the kernel names the file behind an open descriptor.
// Pass: returns sizeof(struct vnode_fdinfowithpath) (1200) with the path, size and type of the file.
#include <fcntl.h>
#include <libproc.h>
#include <stdio.h>
#include <string.h>
#include <sys/proc_info.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	const char *path = "/usr/lib/libSystem.B.dylib";
	struct vnode_fdinfowithpath info;
	struct stat st;
	int fd = open(path, O_RDONLY), bad = 0, p[2];
	if (fd < 0 || fstat(fd, &st)) { puts("FAIL: open"); return 1; }
	lseek(fd, 7, SEEK_SET);
	memset(&info, 0xAA, sizeof(info));
	int n = proc_pidfdinfo(getpid(), fd, PROC_PIDFDVNODEPATHINFO, &info, sizeof(info));
	printf("returned %d (want %zu)\n", n, sizeof(info));
	if (n != (int)sizeof(info)) { puts("FAIL: size"); return 1; }
	printf("path '%s' size %lld type %d fi_type %d offset %lld openflags 0x%x\n", info.pvip.vip_path,
		(long long)info.pvip.vip_vi.vi_stat.vst_size, info.pvip.vip_vi.vi_type, info.pfi.fi_type,
		(long long)info.pfi.fi_offset, info.pfi.fi_openflags);
	bad |= strcmp(info.pvip.vip_path, path) != 0;
	bad |= info.pvip.vip_vi.vi_stat.vst_size != st.st_size || info.pvip.vip_vi.vi_stat.vst_ino != st.st_ino;
	bad |= info.pvip.vip_vi.vi_type != 1 /* VREG */ || info.pfi.fi_type != 1 /* DTYPE_VNODE */;
	bad |= info.pfi.fi_offset != 7 || info.pfi.fi_openflags != 1 /* FREAD */;
	// errors: closed descriptor, short buffer, and a descriptor that is not a file
	bad |= proc_pidfdinfo(getpid(), 999, PROC_PIDFDVNODEPATHINFO, &info, sizeof(info)) > 0;
	bad |= proc_pidfdinfo(getpid(), fd, PROC_PIDFDVNODEPATHINFO, &info, 100) > 0;
	pipe(p);
	bad |= proc_pidfdinfo(getpid(), p[0], PROC_PIDFDVNODEPATHINFO, &info, sizeof(info)) > 0;
	puts(bad ? "FAIL: wrong contents or an error case succeeded" : "PASS");
	return bad;
}
