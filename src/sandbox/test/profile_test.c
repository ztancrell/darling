// Check for the sandbox profile reader. Plain C, runs on the build machine:
//   cc -DSTANDALONE=0 -DUSE_DL=0 -DUSE_MATH=0 -DINLINE= -o /tmp/profile_test test/profile_test.c profile.c tinyscheme/scheme.c
//   /tmp/profile_test
// With arguments, evaluates a profile file and prints its rules:
//   /tmp/profile_test <profile file> [NAME VALUE]...
#include "../profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failed;
#define CHECK(cond, what) do { int ok_ = (cond); printf("%s %s\n", ok_ ? "ok  " : "FAIL", what); if (!ok_) failed = 1; } while (0)

static int has(const struct sbx_profile *p, int allow, const char *operation, const char *filter, const char *value) {
	for (size_t i = 0; i < p->count; i++) {
		const struct sbx_rule *r = &p->rules[i];
		if (r->allow == allow && !strcmp(r->operation, operation) && (filter ? (r->filter && !strcmp(r->filter, filter)) : !r->filter) && (value ? (r->value && !strcmp(r->value, value)) : 1)) {
			return 1;
		}
	}
	return 0;
}

// the constructs Chromium's profiles use
static const char sample[] =
	"(version 1)\n"
	"(deny default)\n"
	"; a comment with a \" quote and #\"not a regex\"\n"
	"(define (param-true? str) (string=? (param str) \"TRUE\"))\n"
	"(define os-version (string->number (param \"OS_VERSION\")))\n"
	"(define (user-homedir-path subpath) (string-append (param \"HOME\") subpath))\n"
	"(allow file-read* file-read-metadata (subpath \"/usr/lib\") (literal \"/dev/null\"))\n"
	"(allow file-read* (regex #\"^/private/var/folders/[^/]+/\\.cache$\"))\n"
	"(allow file-read* (subpath (user-homedir-path \"/Library/Fonts\")))\n"
	"(if (>= os-version 1013) (allow mach-lookup (global-name \"new.service\")))\n"
	"(if (< os-version 1013) (allow mach-lookup (global-name \"old.service\")))\n"
	"(if (param-true? \"ENABLE_X\") (allow sysctl-read (sysctl-name \"hw.x\")))\n"
	"(if (param-true? \"ENABLE_Y\") (allow sysctl-read (sysctl-name \"hw.y\")))\n"
	"(when (defined? 'syscall-unix-not-a-thing) (allow process-exec))\n"
	"(let ((dir (subpath (string-append (param \"CACHE\") \"/metal\")))) (deny file-read* dir))\n"
	"(let loop ((i 0)) (if (< i 3) (begin (allow file* (subpath (param (string-append \"DIR_\" (number->string i))))) (loop (+ i 1)))))\n"
	"(allow file-read-data (require-all (subpath \"/dev\") (vnode-type CHARACTER-DEVICE)))\n"
	"(allow network-outbound (remote tcp \"*:443\"))\n"
	"(deny signal (with no-log))\n"
	// things this reader has never heard of
	"(allow some-new-operation (subpath \"/secret-a\"))\n"
	"(allow file-read* (some-new-filter \"/secret-b\"))\n"
	"(some-new-wrapper (allow file-read* (subpath \"/secret-c\")))\n"
	"(allow file-write* (literal \"/after-the-unknown-words\"))\n";

static const char *const sample_params[] = {
	"OS_VERSION", "1016", "HOME", "/Users/me", "ENABLE_X", "TRUE", "ENABLE_Y", "FALSE", "CACHE", "/var/cache",
	"DIR_0", "/d0", "DIR_1", "/d1", "DIR_2", "/d2", NULL,
};

int main(int argc, char **argv) {
	struct sbx_profile p;
	char *error = NULL;

	if (argc > 1) {
		FILE *f = fopen(argv[1], "rb");
		if (!f) { perror(argv[1]); return 2; }
		static char text[1 << 20];
		text[fread(text, 1, sizeof(text) - 1, f)] = '\0';
		fclose(f);
		if (sbx_profile_evaluate(text, (const char *const *)(argc > 2 ? argv + 2 : NULL), &p, &error) != 0) {
			printf("error: %s\n", error);
			return 1;
		}
		for (size_t i = 0; i < p.count; i++) {
			printf("%s %s %s %s\n", p.rules[i].allow ? "allow" : "deny ", p.rules[i].operation, p.rules[i].filter ? p.rules[i].filter : "-", p.rules[i].value ? p.rules[i].value : "");
		}
		printf("%zu rules\n", p.count);
		return 0;
	}

	CHECK(sbx_profile_evaluate(sample, sample_params, &p, &error) == 0, "sample profile evaluates");
	if (failed) { printf("  error: %s\n", error); return 1; }
	CHECK(p.count > 0 && !p.rules[0].allow && !strcmp(p.rules[0].operation, "default") && !p.rules[0].filter, "first rule is (deny default)");
	CHECK(has(&p, 1, "file-read*", "subpath", "/usr/lib") && has(&p, 1, "file-read-metadata", "literal", "/dev/null"), "several operations and filters give one rule per combination");
	CHECK(has(&p, 1, "file-read*", "regex", "^/private/var/folders/[^/]+/\\.cache$"), "regex literal kept exactly, backslash included");
	CHECK(has(&p, 1, "file-read*", "subpath", "/Users/me/Library/Fonts"), "profile-defined function and parameter");
	CHECK(has(&p, 1, "mach-lookup", "global-name", "new.service") && !has(&p, 1, "mach-lookup", "global-name", "old.service"), "version check picks one branch");
	CHECK(has(&p, 1, "sysctl-read", "sysctl-name", "hw.x") && !has(&p, 1, "sysctl-read", "sysctl-name", "hw.y"), "parameter switch");
	CHECK(!has(&p, 1, "process-exec", NULL, NULL), "guarded by (defined? ...) of something undefined: skipped");
	CHECK(has(&p, 0, "file-read*", "subpath", "/var/cache/metal"), "deny with a let-bound filter");
	CHECK(has(&p, 1, "file*", "subpath", "/d0") && has(&p, 1, "file*", "subpath", "/d2"), "loop over numbered parameters");
	CHECK(has(&p, 1, "file-read-data", "require-all", "(%filter subpath /dev) (%filter vnode-type character-device)"), "compound filter kept as text");
	CHECK(has(&p, 1, "network-outbound", "remote", "tcp *:443"), "filter with bare words");
	CHECK(has(&p, 0, "signal", NULL, NULL), "modifier ignored");
	CHECK(!has(&p, 1, "file-read*", "subpath", "/secret-a") && !has(&p, 1, "some-new-operation", "subpath", NULL), "rule with an unknown operation left out");
	int leaked = 0;
	for (size_t i = 0; i < p.count; i++) {
		if (p.rules[i].value && strstr(p.rules[i].value, "secret")) leaked = 1;
		if (p.rules[i].allow && !p.rules[i].filter && !strcmp(p.rules[i].operation, "file-read*")) leaked = 1;
	}
	CHECK(!leaked, "nothing from the three unknown-word rules got through, and none became an unfiltered allow");
	CHECK(has(&p, 1, "file-write*", "literal", "/after-the-unknown-words"), "rules after unknown words are still read");
	sbx_profile_free(&p);

	CHECK(sbx_profile_evaluate("(allow file-read* (subpath \"/x\")", NULL, &p, &error) != 0 && error, "unbalanced profile is an error");
	if (error) printf("  message: %s\n", error);
	free(error);
	error = NULL;
	CHECK(sbx_profile_evaluate("(allow file-read* (subpath (string-append 1 2)))", NULL, &p, &error) != 0 && error, "profile that fails while running is an error");
	if (error) printf("  message: %s\n", error);
	free(error);

	printf(failed ? "RESULT: FAIL\n" : "RESULT: PASS\n");
	return failed;
}
