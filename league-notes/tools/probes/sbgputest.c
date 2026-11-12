// Apply Chromium's GPU-process sandbox profile (text beside this program) exactly as its helper does,
// with the parameters it passes, to see whether Darling's sandbox survives evaluating it.
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int sandbox_init_with_parameters(const char *profile, uint64_t flags, const char *const parameters[], char **errorbuf);
int sandbox_check(pid_t pid, const char *operation, int type, ...);
int main(int argc, char **argv) {
	static char text[1 << 20]; int fd = open(argv[1], O_RDONLY); if (fd < 0) { perror(argv[1]); return 2; }
	text[read(fd, text, sizeof text - 1)] = '\0'; close(fd);
	const char *const params[] = { "BROWSER_PID", "100", "BUNDLE_ID", "com.riotgames.RiotClient", "BUNDLE_PATH", "/Users/Shared/Riot Games/Riot Client.app", "COMPONENT_PATH", "/tmp/c", "CURRENT_PID", "200", "DARWIN_USER_CACHE_DIR", "/private/var/folders/xx/C", "DARWIN_USER_DIR", "/private/var/folders/xx", "DARWIN_USER_TEMP_DIR", "/private/var/folders/xx/T", "DISABLE_METAL_SHADER_CACHE", "FALSE", "DISABLE_SANDBOX_DENIAL_LOGGING", "TRUE", "ENABLE_LOGGING", "FALSE", "EXECUTABLE_PATH", "/Users/Shared/Riot Games/Riot Client.app/Contents/MacOS/Riot Client", "USER_HOMEDIR_AS_LITERAL", "/Users/zpt", "LOG_FILE_PATH", "/tmp/log", "OS_VERSION", "1016", "FILTER_SYSCALLS", "FALSE", "FILTER_SYSCALLS_DEBUG", "FALSE", NULL };
	char *error = NULL; printf("profile: %zu bytes; applying...\n", strlen(text)); fflush(stdout);
	int rc = sandbox_init_with_parameters(text, 0, params, &error);
	printf("sandbox_init_with_parameters: %d%s%s; sandboxed=%d\n", rc, error ? " error: " : "", error ? error : "", sandbox_check(getpid(), NULL, 0));
	printf(rc == 0 ? "RESULT: PASS\n" : "RESULT: FAIL\n"); return rc != 0;
}
