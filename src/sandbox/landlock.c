// Enforces the file rules of a sandbox profile with Linux's Landlock.
//
// Landlock is an allow-list of file hierarchies, checked by the Linux kernel, that a process applies
// to itself and can never remove. That is the shape of a `(deny default)` profile, but it is not as
// expressive: no regular expressions, no "deny" inside something allowed, no single directory
// without what is beneath it, and only files that already exist. Rules that do not fit are counted
// and reported, not enforced. Everything that is not a file rule (services, devices, network) is
// outside Landlock entirely.

#include "profile.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

// from libsystem_kernel
extern int __linux_syscall(int nr, long a1, long a2, long a3, long a4, long a5, long a6);
extern int __darling_vchroot_expand(const char *path, char *out);

// Linux system call numbers and constants; this file is built for macOS, so there are no Linux headers
#define LINUX_NR_landlock_create_ruleset 444
#define LINUX_NR_landlock_add_rule 445
#define LINUX_NR_landlock_restrict_self 446
#if defined(__x86_64__)
#define LINUX_NR_openat 257
#define LINUX_NR_prctl 157
#define LINUX_NR_close 3
#elif defined(__i386__)
#define LINUX_NR_openat 295
#define LINUX_NR_prctl 172
#define LINUX_NR_close 6
#elif defined(__arm64__) || defined(__aarch64__)
#define LINUX_NR_openat 56
#define LINUX_NR_prctl 167
#define LINUX_NR_close 57
#else
#error "Missing Linux system call numbers for this architecture"
#endif

#define LINUX_AT_FDCWD (-100)
#define LINUX_O_PATH 010000000
#define LINUX_O_CLOEXEC 02000000
#define LINUX_PR_SET_NO_NEW_PRIVS 38
#define LINUX_ENOSYS 38
#define LINUX_EOPNOTSUPP 95
#define LINUX_EINVAL 22

#define LANDLOCK_CREATE_RULESET_VERSION (1U << 0)
#define LANDLOCK_RULE_PATH_BENEATH 1
#define LANDLOCK_RESTRICT_SELF_TSYNC (1U << 3)

#define FS_EXECUTE (1ULL << 0)
#define FS_WRITE_FILE (1ULL << 1)
#define FS_READ_FILE (1ULL << 2)
#define FS_READ_DIR (1ULL << 3)
#define FS_REMOVE_DIR (1ULL << 4)
#define FS_REMOVE_FILE (1ULL << 5)
#define FS_MAKE_DIR (1ULL << 7)
#define FS_MAKE_REG (1ULL << 8)
#define FS_MAKE_SYM (1ULL << 12)
#define FS_REFER (1ULL << 13)    // Landlock ABI 2
#define FS_TRUNCATE (1ULL << 14) // Landlock ABI 3

#define FS_READ (FS_READ_FILE | FS_READ_DIR)
#define FS_WRITE (FS_WRITE_FILE | FS_TRUNCATE | FS_MAKE_REG | FS_MAKE_DIR | FS_MAKE_SYM | FS_REMOVE_FILE | FS_REMOVE_DIR | FS_REFER)
// the rights that make sense on something that is not a directory
#define FS_FILE_RIGHTS (FS_EXECUTE | FS_WRITE_FILE | FS_READ_FILE | FS_TRUNCATE)

struct landlock_ruleset_attr {
	uint64_t handled_access_fs;
};

struct landlock_path_beneath_attr {
	uint64_t allowed_access;
	int32_t parent_fd;
} __attribute__((packed));

// what a profile operation needs from Landlock; 0 with known == true means "nothing to enforce"
static uint64_t rights_for_operation(const char *operation, bool *known) {
	static const struct {
		const char *operation;
		uint64_t rights;
	} table[] = {
		{ "file*", FS_READ | FS_WRITE },
		{ "file-read*", FS_READ },
		{ "file-read-data", FS_READ },
		{ "file-map-executable", FS_READ_FILE },
		{ "file-write*", FS_WRITE },
		{ "file-write-data", FS_WRITE_FILE | FS_TRUNCATE },
		{ "file-write-create", FS_MAKE_REG | FS_MAKE_DIR | FS_MAKE_SYM },
		{ "file-write-unlink", FS_REMOVE_FILE | FS_REMOVE_DIR },
		{ "process-exec", FS_EXECUTE | FS_READ_FILE },
		{ "process-exec*", FS_EXECUTE | FS_READ_FILE },
		// Landlock never restricts looking at a file's attributes or walking through directories
		{ "file-read-metadata", 0 },
		{ "file-read-xattr", 0 },
		{ "file-test-existence", 0 },
		{ "file-search", 0 },
	};
	for (size_t i = 0; i < sizeof(table) / sizeof(*table); i++) {
		if (strcmp(table[i].operation, operation) == 0) {
			*known = true;
			return table[i].rights;
		}
	}
	*known = false;
	return 0;
}

// What a Darling process needs that no macOS profile mentions: on macOS the system libraries come from
// a shared cache, not files, and Linux's own /proc, /sys and /dev do not exist there.
// This list is a first guess; extend it from what confined processes are actually refused.
static const struct {
	const char *path; // a macOS path, as the process sees it
	uint64_t rights;
} darling_needs[] = {
	{ "/usr/lib", FS_READ | FS_EXECUTE },
	{ "/usr/share", FS_READ },
	{ "/System/Library", FS_READ | FS_EXECUTE },
	{ "/Volumes/SystemRoot/proc", FS_READ },
	{ "/Volumes/SystemRoot/sys", FS_READ },
	{ "/Volumes/SystemRoot/dev", FS_READ | FS_WRITE_FILE },
	{ "/Volumes/SystemRoot/dev/shm", FS_READ | FS_WRITE_FILE | FS_TRUNCATE | FS_MAKE_REG | FS_REMOVE_FILE },
	// host libraries behind Darling's wrappers (X11, fontconfig, the Vulkan driver, ...) and their data
	{ "/Volumes/SystemRoot/usr/lib", FS_READ | FS_EXECUTE },
	{ "/Volumes/SystemRoot/usr/lib64", FS_READ | FS_EXECUTE },
	{ "/Volumes/SystemRoot/usr/share", FS_READ },
	{ "/Volumes/SystemRoot/etc", FS_READ },
};

enum skip_reason {
	skip_not_file_rule,
	skip_deny,
	skip_filter,
	skip_single_directory,
	skip_missing,
	skip_reason_count,
};

static const char *const skip_descriptions[skip_reason_count] = {
	[skip_not_file_rule] = "rules that are not about files (services, devices, network, ...): not enforced",
	[skip_deny] = "deny rules: Landlock cannot carve exceptions out of something allowed, so these are more open than the profile",
	[skip_filter] = "allow rules matched by a regular expression or another filter Landlock cannot express: not allowed, so stricter than the profile",
	[skip_single_directory] = "allow rules for one directory without its contents: not allowed, so stricter than the profile",
	[skip_missing] = "allow rules for paths that do not exist (yet): not allowed, so stricter than the profile",
};

static bool verbose(void) {
	return getenv("DARLING_SANDBOX_VERBOSE") != NULL;
}

// adds "allow `rights` on `path` and everything beneath it"; returns false if the path cannot be used
static bool add_path(int ruleset, const char *path, uint64_t rights, uint64_t handled) {
	char resolved[MAXPATHLEN];
	char linux_path[4096];
	struct stat st;
	bool ok = false;

	// resolve symbolic links as macOS paths first (/tmp is /private/tmp); their targets mean nothing to Linux
	if (!realpath(path, resolved) || strlen(resolved) >= sizeof(linux_path) || __darling_vchroot_expand(resolved, linux_path) != 0) {
		return false;
	}

	int fd = __linux_syscall(LINUX_NR_openat, LINUX_AT_FDCWD, (long)linux_path, LINUX_O_PATH | LINUX_O_CLOEXEC, 0, 0, 0);
	if (fd < 0) {
		return false;
	}

	rights &= handled;
	if (fstat(fd, &st) == 0) {
		if (!S_ISDIR(st.st_mode)) {
			rights &= FS_FILE_RIGHTS;
		}
		struct landlock_path_beneath_attr attr = { .allowed_access = rights, .parent_fd = fd };
		// a rule with no rights left (say, "create files" on a plain file) is refused by the kernel and not needed
		ok = rights == 0 || __linux_syscall(LINUX_NR_landlock_add_rule, ruleset, LANDLOCK_RULE_PATH_BENEATH, (long)&attr, 0, 0, 0) == 0;
	}

	__linux_syscall(LINUX_NR_close, fd, 0, 0, 0, 0, 0);
	return ok;
}

static bool is_directory(const char *path) {
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int sbx_landlock_apply(const struct sbx_profile *profile, char **error) {
	char message[256] = "";
	size_t skipped[skip_reason_count] = { 0 };
	size_t installed = 0;
	size_t approximate = 0;
	int ruleset = -1;
	int result = -1;

	int abi = __linux_syscall(LINUX_NR_landlock_create_ruleset, 0, 0, LANDLOCK_CREATE_RULESET_VERSION, 0, 0, 0);
	if (abi < 1) {
		snprintf(message, sizeof(message), "the Linux kernel has no usable Landlock (%s); cannot sandbox", (abi == -LINUX_ENOSYS) ? "not built in" : (abi == -LINUX_EOPNOTSUPP) ? "disabled at boot" : "unexpected error");
		goto out;
	}

	// only rights named here are restricted at all; rights from newer kernels must not be named on older ones
	struct landlock_ruleset_attr ruleset_attr = {
		.handled_access_fs = FS_EXECUTE | FS_WRITE_FILE | FS_READ_FILE | FS_READ_DIR | FS_REMOVE_DIR | FS_REMOVE_FILE | FS_MAKE_DIR | FS_MAKE_REG | FS_MAKE_SYM | (abi >= 2 ? FS_REFER : 0) | (abi >= 3 ? FS_TRUNCATE : 0),
	};
	uint64_t handled = ruleset_attr.handled_access_fs;

	ruleset = __linux_syscall(LINUX_NR_landlock_create_ruleset, (long)&ruleset_attr, sizeof(ruleset_attr), 0, 0, 0, 0);
	if (ruleset < 0) {
		snprintf(message, sizeof(message), "could not create a Landlock ruleset (Linux error %d)", -ruleset);
		goto out;
	}

	for (size_t i = 0; i < profile->count; i++) {
		const struct sbx_rule *rule = &profile->rules[i];
		enum skip_reason reason;
		bool known_operation;
		uint64_t rights = rights_for_operation(rule->operation, &known_operation);

		if (strcmp(rule->operation, "default") == 0 && !rule->allow) {
			continue; // (deny default) is what Landlock does anyway
		} else if (!known_operation) {
			reason = skip_not_file_rule;
		} else if (rights == 0) {
			continue;
		} else if (!rule->allow) {
			reason = skip_deny;
		} else if (!rule->filter || (strcmp(rule->filter, "subpath") != 0 && strcmp(rule->filter, "path") != 0 && strcmp(rule->filter, "literal") != 0)) {
			// ponytail: an allow with no filter at all means "any file"; it is counted here and so not allowed.
			// Give it "/" if a profile turns out to rely on it.
			reason = skip_filter;
		} else if (strcmp(rule->filter, "subpath") != 0 && is_directory(rule->value)) {
			reason = skip_single_directory;
		} else if (!add_path(ruleset, rule->value, rights, handled)) {
			reason = skip_missing;
		} else {
			installed++;
			if (rule->approximate) {
				approximate++;
			}
			if (verbose()) {
				fprintf(stderr, "DARLING sandbox: allowed %s on %s%s\n", rule->operation, rule->value, rule->approximate ? " (the profile's extra conditions are not checked)" : "");
			}
			continue;
		}

		skipped[reason]++;
		if (verbose()) {
			fprintf(stderr, "DARLING sandbox: not enforced: %s %s %s %s\n", rule->allow ? "allow" : "deny", rule->operation, rule->filter ? rule->filter : "", rule->value ? rule->value : "");
		}
	}

	for (size_t i = 0; i < sizeof(darling_needs) / sizeof(*darling_needs); i++) {
		bool added = add_path(ruleset, darling_needs[i].path, darling_needs[i].rights, handled);
		fprintf(stderr, "DARLING sandbox: %s for Darling itself, not in the profile: %s\n", added ? "also allowed" : "could not allow", darling_needs[i].path);
	}

	int status = __linux_syscall(LINUX_NR_prctl, LINUX_PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0, 0);
	if (status < 0) {
		snprintf(message, sizeof(message), "could not set no_new_privs (Linux error %d)", -status);
		goto out;
	}

	// every thread of the process at once where the kernel can; otherwise this thread and the ones it starts later
	bool all_threads = true;
	status = __linux_syscall(LINUX_NR_landlock_restrict_self, ruleset, LANDLOCK_RESTRICT_SELF_TSYNC, 0, 0, 0, 0);
	if (status == -LINUX_EINVAL) {
		all_threads = false;
		status = __linux_syscall(LINUX_NR_landlock_restrict_self, ruleset, 0, 0, 0, 0, 0);
	}
	if (status < 0) {
		snprintf(message, sizeof(message), "could not apply the Landlock ruleset (Linux error %d)", -status);
		goto out;
	}

	fprintf(stderr, "DARLING sandbox: file access confined by Landlock (ABI %d, %s): %zu rule(s) from the profile%s\n", abi, all_threads ? "all threads" : "this thread and threads it starts; threads already running are NOT confined", installed, verbose() ? "" : "; set DARLING_SANDBOX_VERBOSE for each rule");
	if (approximate) {
		fprintf(stderr, "DARLING sandbox: %zu of those had extra conditions that are not checked, so they are more open than the profile\n", approximate);
	}
	for (int reason = 0; reason < skip_reason_count; reason++) {
		if (skipped[reason]) {
			fprintf(stderr, "DARLING sandbox: %zu %s\n", skipped[reason], skip_descriptions[reason]);
		}
	}
	result = 0;

out:
	if (ruleset >= 0) {
		__linux_syscall(LINUX_NR_close, ruleset, 0, 0, 0, 0, 0);
	}
	if (result != 0 && error) {
		*error = strdup(message);
	}
	return result;
}
