#ifndef _DARLING_SANDBOX_PROFILE_H_
#define _DARLING_SANDBOX_PROFILE_H_

#include <stdbool.h>
#include <stddef.h>

// One rule produced by evaluating a sandbox profile: `(allow file-read* (subpath "/usr/lib"))`
// becomes { true, "file-read*", "subpath", "/usr/lib" }. A rule with several operations and
// filters becomes one entry per combination; a rule with no filter has filter == NULL.
struct sbx_rule {
	bool allow;
	char *operation;
	char *filter; // filter kind ("subpath", "literal", "global-name", ...), or NULL
	char *value;  // the filter's argument(s) as text, or NULL
	// The profile attached more conditions to this path than `filter` says, as in
	// (require-all (path "/x") (vnode-type REGULAR-FILE)); only the path is kept.
	bool approximate;
};

struct sbx_profile {
	struct sbx_rule *rules;
	size_t count;
};

// Evaluates `profile` (a Scheme program in Apple's sandbox profile language) and collects its rules
// in order. `params` is a NULL-terminated list of alternating names and values, as passed to
// sandbox_init_with_parameters(), or NULL. Words the profile uses that are not known here are
// reported on stderr and the rules that depend on them are left out.
// Returns 0 on success. On failure returns -1 and, if `error` is not NULL, sets *error to a
// malloc'd message.
int sbx_profile_evaluate(const char *profile, const char *const params[], struct sbx_profile *out, char **error);

void sbx_profile_free(struct sbx_profile *profile);

// Confines the calling process to the file rules of `profile`, using Landlock. Cannot be undone.
// Returns 0 on success. On failure returns -1 and, if `error` is not NULL, sets *error to a
// malloc'd message; the process is then not confined.
int sbx_landlock_apply(const struct sbx_profile *profile, char **error);

#endif
