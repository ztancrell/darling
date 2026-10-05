#include "sandbox.h"
#include "profile.h"
#include <stdbool.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

// DUMMY implementation
const enum sandbox_filter_type SANDBOX_CHECK_NO_REPORT = SANDBOX_FILTER_NONE;

// set once this process has confined itself; sandbox_check(getpid(), NULL, ...) reports it
static bool sandboxed;

// ponytail: Apple's named profiles (kSBXProfileNoInternet, ...) and sandbox extensions are not
// implemented. A request for a named profile reports success without restricting anything, as all
// sandbox requests did before; it is logged. Real fix: write those profiles.
static int pretend_sandbox_init(const char *function, char **errorbuf)
{
	fprintf(stderr, "DARLING %s: named profiles are not implemented; reporting success without restricting this process\n", function);
	if (errorbuf)
		*errorbuf = NULL;
	return 0;
}

// Evaluates the profile and confines the process's file access to it (see landlock.c for what is and
// is not enforced). macOS only sets *errorbuf on failure, and callers treat a non-NULL error as failure.
static int real_sandbox_init(const char *profile, const char *const parameters[], char **errorbuf)
{
	struct sbx_profile rules;
	char *error = NULL;
	int result = -1;

	if (errorbuf)
		*errorbuf = NULL;

	if (!profile) {
		error = strdup("no profile given");
	} else if (sbx_profile_evaluate(profile, parameters, &rules, &error) == 0) {
		result = sbx_landlock_apply(&rules, &error);
		sbx_profile_free(&rules);
	}

	if (result == 0) {
		sandboxed = true;
	} else {
		fprintf(stderr, "DARLING sandbox: failed, process is not sandboxed: %s\n", error ? error : "unknown error");
		if (errorbuf)
			*errorbuf = error;
		else
			free(error);
	}
	return result;
}

int sandbox_init(const char *profile, uint64_t flags, char **errorbuf)
{
	// SANDBOX_NAMED: `profile` is the name of a built-in profile, not profile text
	if (flags & SANDBOX_NAMED)
		return pretend_sandbox_init("sandbox_init", errorbuf);
	return real_sandbox_init(profile, NULL, errorbuf);
}

const char kSBXProfileNoInternet[] = "no_internet";

const char kSBXProfileNoNetwork[] = "no_network";

const char kSBXProfileNoWrite[] = "no_write";

const char kSBXProfileNoWriteExceptTemporary[] = "no_write_except_temporary";

const char kSBXProfilePureComputation[] = "pure_computation";

const char APP_SANDBOX_IOKIT_CLIENT[] = "com.apple.app-sandbox.iokit-client";
const char APP_SANDBOX_MACH[] = "com.apple.app-sandbox.mach";
const char APP_SANDBOX_READ[] = "com.apple.app-sandbox.read";
const char APP_SANDBOX_READ_WRITE[] = "com.apple.app-sandbox.read-write";
const char IOS_SANDBOX_APPLICATION_GROUP[] = "com.apple.sandbox.application-group";
const char IOS_SANDBOX_CONTAINER[] = "com.apple.sandbox.container";

// Assuming these are 64-bit only because at least one has a value
// that can't be stored in a 32-bit integer.
#ifdef __LP64__

const uint64_t SANDBOX_EXTENSION_CANONICAL = 0x0000001000000002;
const uint64_t SANDBOX_EXTENSION_DEFAULT = 0x0000000400000000;
const uint64_t SANDBOX_EXTENSION_MAGIC = 0x0000000000000001;
const uint64_t SANDBOX_EXTENSION_NO_REPORT = 0x0000000100000010;
const uint64_t SANDBOX_EXTENSION_PREFIXMATCH = 0x0000000200000004;
const uint64_t SANDBOX_EXTENSION_UNRESOLVED = 0x0000000100000001;

#endif

void sandbox_free_error(char *errorbuf)
{
	free(errorbuf);
}

int sandbox_init_with_parameters(const char *profile, uint64_t flags, const char *const parameters[], char **errorbuf)
{
	if (flags & SANDBOX_NAMED)
		return pretend_sandbox_init("sandbox_init_with_parameters", errorbuf);
	return real_sandbox_init(profile, parameters, errorbuf);
}

int sandbox_init_with_extensions(const char *profile, uint64_t flags, const char *const extensions[], char **errorbuf)
{
	if (flags & SANDBOX_NAMED)
		return pretend_sandbox_init("sandbox_init_with_extensions", errorbuf);
	// the extensions are not honoured: rules that depend on them are simply not allowed
	return real_sandbox_init(profile, NULL, errorbuf);
}

// With no operation: 1 if the process is sandboxed, 0 if not. With an operation: 0 if it is
// allowed, 1 if denied.
// ponytail: only knows about this process, and answers "allowed" to every operation, including
// file operations Landlock will in fact refuse. Real fix: answer from the evaluated profile.
int sandbox_check(pid_t pid, const char *operation, enum sandbox_filter_type type, ...)
{
	if (!operation && pid == getpid())
		return sandboxed;
	return 0;
}

int sandbox_note(const char *note)
{
	printf("%s\n", note);
	return 0;
}

int sandbox_suspend(pid_t pid)
{
	return -1;
}

int sandbox_unsuspend(void)
{
	return -1;
}

int sandbox_issue_extension(const char *path, char **ext_token)
{
	return -1;
}

int sandbox_issue_fs_extension(const char *path, uint64_t flags, char **ext_token)
{
	return -1;
}

int sandbox_issue_fs_rw_extension(const char *path, char **ext_token)
{
	return -1;
}

int sandbox_issue_mach_extension(const char *name, char **ext_token)
{
	return -1;
}

int sandbox_consume_extension(const char *path, const char *ext_token)
{
	return -1;
}

int sandbox_consume_fs_extension(const char *ext_token, char **path)
{
	return -1;
}

int sandbox_consume_mach_extension(const char *ext_token, char **name)
{
	return -1;
}

int sandbox_release_fs_extension(const char *ext_token)
{
	return -1;
}

int sandbox_container_path_for_pid(pid_t pid, char *buffer, size_t bufsize)
{
	return -1;
}

int sandbox_wakeup_daemon(char **errorbuf)
{
	*errorbuf = strdup("Not implemented");
	return -1;
}

const char *_amkrtemp(const char *path)
{
	size_t len = strlen(path);
	const char suffix[] = ".amkrtempXXXXXX";
	char *template = malloc(len + sizeof(suffix));
	memcpy(template, path, len);
	memcpy(template + len, suffix, sizeof(suffix));
	return mktemp(template);
}

int rootless_allows_task_for_pid(pid_t pid) {
	return 1;
}

int sandbox_check_by_audit_token(audit_token_t tok, const char* operation, enum sandbox_filter_type filt, ...) {
	// technically unimplemented
	// just return 0 as an indiciation that whatever operation the caller asked about is allowed
	return 0;
};

int rootless_check_trusted(const char* path) {
	// also unimplemented
	// Darling doesn't have rootless anyways so just return a value indicating trust
	return 0;
};

int rootless_restricted_environment() {
	// also unimplemented
	// Darling doesn't have rootless anyways, so just return a value indicating an unrestricted environment
	return 0;
};

int rootless_check_datavault_flag(const char* path, const char* storage_class) {
	// also unimplemented
	// not quite sure what a "data vault" is here, but its usage in `JSScript.mm` in JavaScriptCore seems to indicate that we should return `0`
	return 0;
};

int sandbox_query_approval_policy_for_path(const char* query, const char* path, char **approval) {
	return -1;
}

/* ---- Darling: local discovery placeholders, NOT implementations ----
 * Each logs its own name when called and returns zero, so one run shows which of these an
 * application actually reaches. Replace each with a real implementation, or remove it. */
#include <stdio.h>
#ifndef DARLING_UNIMPLEMENTED
#define DARLING_UNIMPLEMENTED(sym) \
	__attribute__((visibility("default"))) long darling_unimplemented_##sym(void) __asm__("_" #sym); \
	long darling_unimplemented_##sym(void) { fprintf(stderr, "DARLING unimplemented function called: " #sym "\n"); return 0; }
#endif
DARLING_UNIMPLEMENTED(responsibility_get_pid_responsible_for_pid)
DARLING_UNIMPLEMENTED(sandbox_extension_consume)
DARLING_UNIMPLEMENTED(sandbox_extension_issue_file)
DARLING_UNIMPLEMENTED(sandbox_extension_release)

/* libSystem is expected to export this compiler runtime call (compiler-rt's atomic.c is not built).
 * ponytail: plain copy, not atomic; build atomic.c if a caller needs the lock. */
#include <string.h>
__attribute__((visibility("default"))) void darling_atomic_load(int size, void *src, void *dest, int model) __asm__("___atomic_load");
void darling_atomic_load(int size, void *src, void *dest, int model) {
	memcpy(dest, src, size);
}
