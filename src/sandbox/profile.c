// Evaluates sandbox profiles. A profile is a Scheme program; running it produces the rules.
// TinyScheme runs the program; this file supplies the sandbox vocabulary (allow, deny, param, the
// filters, the operation names), written from the public profile language. Nothing is enforced here.

#include "profile.h"
#include "tinyscheme/scheme.h"
#include "tinyscheme/scheme-private.h"
#include "tinyscheme/init_scm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_UNKNOWN_WORDS 64
#define ERROR_BUFFER_SIZE 512

struct state {
	const char *const *params;
	struct sbx_profile *out;
	bool quiet; // a pass that will be repeated; do not report left-out rules twice
	bool out_of_memory;
};

static const char *const operations[] = {
	"default",
	"appleevent-send", "authorization-right-obtain", "boot-arg-set", "device*", "device-camera", "device-microphone",
	"distributed-notification-post", "dynamic-code-generation", "file*", "file-chroot", "file-clone", "file-ioctl",
	"file-issue-extension", "file-link", "file-lock", "file-map-executable", "file-mknod", "file-mount",
	"file-mount-update", "file-read*", "file-read-data", "file-read-metadata", "file-read-xattr", "file-revoke",
	"file-search", "file-test-existence", "file-unmount", "file-write*", "file-write-acl", "file-write-create",
	"file-write-data", "file-write-finderinfo", "file-write-flags", "file-write-mode", "file-write-mount",
	"file-write-owner", "file-write-setugid", "file-write-times", "file-write-unlink", "file-write-xattr",
	"generic-issue-extension", "hid-control", "iokit*", "iokit-issue-extension", "iokit-open",
	"iokit-open-user-client", "iokit-open-service", "iokit-get-properties", "iokit-set-properties", "ipc*",
	"ipc-posix*", "ipc-posix-issue-extension", "ipc-posix-sem", "ipc-posix-sem*", "ipc-posix-sem-create",
	"ipc-posix-sem-open", "ipc-posix-sem-post", "ipc-posix-sem-unlink", "ipc-posix-sem-wait", "ipc-posix-shm",
	"ipc-posix-shm*", "ipc-posix-shm-read*", "ipc-posix-shm-read-data", "ipc-posix-shm-read-metadata",
	"ipc-posix-shm-write*", "ipc-posix-shm-write-create", "ipc-posix-shm-write-data", "ipc-posix-shm-write-unlink",
	"ipc-sysv*", "ipc-sysv-msg", "ipc-sysv-sem", "ipc-sysv-shm", "job-creation", "load-unsigned-code",
	"lsopen", "mach*", "mach-bootstrap", "mach-cross-domain-lookup", "mach-host*", "mach-host-exception-port-set",
	"mach-host-special-port-set", "mach-issue-extension", "mach-lookup", "mach-per-user-lookup", "mach-priv*",
	"mach-priv-host-port", "mach-priv-task-port", "mach-register", "mach-task-name", "network*",
	"network-inbound", "network-bind", "network-outbound", "nvram*", "nvram-delete", "nvram-get", "nvram-set",
	"process*", "process-codesigning-status*", "process-exec", "process-exec*", "process-exec-interpreter",
	"process-fork", "process-info*", "process-info-codesignature", "process-info-dirtycontrol",
	"process-info-listpids", "process-info-pidfdinfo", "process-info-pidfileportinfo", "process-info-pidinfo",
	"process-info-rusage", "process-info-setcontrol", "pseudo-tty", "signal", "socket-ioctl", "socket-option*",
	"socket-option-get", "socket-option-set", "syscall*", "syscall-mach", "syscall-mig", "syscall-unix", "sysctl*",
	"sysctl-read", "sysctl-write", "system*", "system-acct", "system-audit", "system-chud", "system-debug",
	"system-fsctl", "system-info", "system-kext*", "system-kext-load", "system-kext-query", "system-kext-unload",
	"system-lcid", "system-mac-label", "system-nfssvc", "system-privilege", "system-reboot", "system-sched",
	"system-set-time", "system-socket", "system-suspend-resume", "system-swap", "user-preference*",
	"user-preference-read", "user-preference-write",
};

// filters whose arguments are expressions (strings, usually built with `param` and `string-append`)
static const char *const evaluated_filters[] = {
	"subpath", "path", "literal", "regex", "prefix", "mount-relative-regex", "mount-relative-path",
	"mount-relative-literal", "global-name", "global-name-regex", "global-name-prefix", "local-name",
	"local-name-regex", "local-name-prefix", "xpc-service-name", "xpc-service-name-regex",
	"xpc-service-name-prefix", "sysctl-name", "sysctl-name-regex", "sysctl-name-prefix",
	"iokit-user-client-class", "iokit-user-client-class-regex", "iokit-registry-entry-class",
	"iokit-registry-entry-class-regex", "iokit-connection", "iokit-property", "iokit-property-regex",
	"ipc-posix-name", "ipc-posix-name-regex", "ipc-posix-name-prefix", "preference-domain",
	"preference-domain-regex", "preference-domain-prefix", "extension", "extension-class", "signing-identifier",
	"appleevent-destination", "right-name", "nvram-variable", "process-path", "process-path-regex",
	"require-all", "require-any", "require-not", "require-entitlement", "entitlement-value",
	"info-type", "notification-name", "kext-bundle-id",
};

// filters and modifiers whose arguments are bare words (`(vnode-type REGULAR-FILE)`, `(remote tcp "*:80")`,
// `(with send-signal SIGSYS)`); they are kept as text, not evaluated
static const char *const quoted_filters[] = {
	"vnode-type", "socket-domain", "socket-type", "socket-protocol", "remote", "local", "remote-type",
	"file-mode", "syscall-number", "mach-message-number", "machtrap-number", "target", "fsctl-command",
	"ioctl-command", "debug-mode", "control-name", "with",
};

static pointer intern(scheme *sc, const char *name) {
	return mk_symbol(sc, name);
}

static bool is_tagged(scheme *sc, pointer p, const char *tag) {
	return is_pair(p) && pair_car(p) == intern(sc, tag);
}

static void append_string(char **text, const char *piece, struct state *state) {
	size_t old_length = *text ? strlen(*text) : 0;
	char *grown = realloc(*text, old_length + strlen(piece) + 1);
	if (!grown) {
		state->out_of_memory = true;
		return;
	}
	strcpy(grown + old_length, piece);
	*text = grown;
}

// strings, symbols, numbers, booleans and lists of those; anything else (a procedure, an unknown
// word) is not something a rule can be made from
static bool is_plain(scheme *sc, pointer p) {
	if (is_tagged(sc, p, "%unknown")) {
		return false;
	}
	for (; is_pair(p); p = pair_cdr(p)) {
		if (!is_plain(sc, pair_car(p))) {
			return false;
		}
	}
	return p == sc->NIL || p == sc->T || p == sc->F || is_string(p) || is_symbol(p) || is_number(p);
}

// appends the text of a plain value to *text (a malloc'd string that grows); strings appear without quotes
static void append_text(scheme *sc, pointer p, char **text, struct state *state) {
	char number[32];

	if (is_pair(p) || p == sc->NIL) {
		append_string(text, "(", state);
		for (bool first = true; is_pair(p); p = pair_cdr(p), first = false) {
			if (!first) {
				append_string(text, " ", state);
			}
			append_text(sc, pair_car(p), text, state);
		}
		append_string(text, ")", state);
	} else if (is_string(p)) {
		append_string(text, string_value(p), state);
	} else if (is_symbol(p)) {
		append_string(text, symname(p), state);
	} else if (is_number(p)) {
		snprintf(number, sizeof(number), "%ld", ivalue(p));
		append_string(text, number, state);
	} else {
		append_string(text, p == sc->F ? "#f" : "#t", state);
	}
}

static void add_rule(struct state *state, bool allow, const char *operation, const char *filter, const char *value, bool approximate) {
	struct sbx_profile *out = state->out;
	struct sbx_rule *grown = realloc(out->rules, (out->count + 1) * sizeof(*grown));
	if (!grown) {
		state->out_of_memory = true;
		return;
	}
	out->rules = grown;

	struct sbx_rule *rule = &out->rules[out->count];
	rule->allow = allow;
	rule->approximate = approximate;
	rule->operation = strdup(operation);
	rule->filter = filter ? strdup(filter) : NULL;
	rule->value = value ? strdup(value) : NULL;
	if (!rule->operation || (filter && !rule->filter) || (value && !rule->value)) {
		free(rule->operation);
		free(rule->filter);
		free(rule->value);
		state->out_of_memory = true;
		return;
	}
	out->count++;
}

// (%rule allow? args): `args` are the evaluated arguments of an `allow` or `deny` form
static pointer ff_rule(scheme *sc, pointer args) {
	struct state *state = sc->ext_data;
	bool allow = pair_car(args) != sc->F;
	pointer items = pair_car(pair_cdr(args));
	size_t operation_count = 0;
	size_t filter_count = 0;

	// a rule that mentions anything we do not understand is left out entirely: guessing at part
	// of an `allow` could permit more than the profile meant
	for (pointer i = items; is_pair(i); i = pair_cdr(i)) {
		pointer item = pair_car(i);
		if (is_plain(sc, item) && is_tagged(sc, item, "%op")) {
			operation_count++;
		} else if (is_plain(sc, item) && is_tagged(sc, item, "%filter")) {
			filter_count++;
		} else if (!is_plain(sc, item) || !is_tagged(sc, item, "%modifier")) {
			if (!state->quiet) {
				fprintf(stderr, "DARLING sandbox profile: one %s rule left out, it uses something not understood\n", allow ? "allow" : "deny");
			}
			return sc->T;
		}
	}

	if (operation_count == 0) {
		return sc->T;
	}

	for (pointer i = items; is_pair(i); i = pair_cdr(i)) {
		pointer operation = pair_car(i);
		if (!is_tagged(sc, operation, "%op")) {
			continue;
		}
		const char *operation_name = symname(pair_cdr(operation));

		if (filter_count == 0) {
			add_rule(state, allow, operation_name, NULL, NULL, false);
			continue;
		}

		for (pointer j = items; is_pair(j); j = pair_cdr(j)) {
			pointer filter = pair_car(j);
			if (!is_tagged(sc, filter, "%filter")) {
				continue;
			}
			// (%filter kind arg...)
			pointer kind = pair_car(pair_cdr(filter));
			pointer filter_args = pair_cdr(pair_cdr(filter));
			bool approximate = false;
			char *value = NULL;

			// (require-all (path "/x") <other conditions>): keep the one path, marked approximate
			if (kind == intern(sc, "require-all")) {
				pointer the_path = sc->NIL;
				size_t path_count = 0;
				for (pointer k = filter_args; is_pair(k); k = pair_cdr(k)) {
					pointer inner = pair_car(k);
					if (!is_tagged(sc, inner, "%filter")) {
						continue;
					}
					pointer inner_kind = pair_car(pair_cdr(inner));
					pointer inner_args = pair_cdr(pair_cdr(inner));
					if ((inner_kind == intern(sc, "path") || inner_kind == intern(sc, "literal") || inner_kind == intern(sc, "subpath")) && is_pair(inner_args) && pair_cdr(inner_args) == sc->NIL && is_string(pair_car(inner_args))) {
						the_path = inner;
						path_count++;
					}
				}
				if (path_count == 1) {
					kind = pair_car(pair_cdr(the_path));
					filter_args = pair_cdr(pair_cdr(the_path));
					approximate = true;
				}
			}

			if (is_pair(filter_args) && pair_cdr(filter_args) == sc->NIL) {
				append_text(sc, pair_car(filter_args), &value, state);
			} else {
				for (pointer k = filter_args; is_pair(k); k = pair_cdr(k)) {
					if (value) {
						append_string(&value, " ", state);
					}
					append_text(sc, pair_car(k), &value, state);
				}
			}
			add_rule(state, allow, operation_name, symname(kind), value ? value : "", approximate);
			free(value);
		}
	}

	return sc->T;
}

// (param "NAME") -> the parameter's value, or #f
static pointer ff_param(scheme *sc, pointer args) {
	struct state *state = sc->ext_data;
	if (!is_pair(args) || !is_string(pair_car(args)) || !state->params) {
		return sc->F;
	}
	for (const char *const *param = state->params; param[0] && param[1]; param += 2) {
		if (strcmp(param[0], string_value(pair_car(args))) == 0) {
			return mk_string(sc, param[1]);
		}
	}
	return sc->F;
}

// (regex-quote "a.b") -> "a\.b"
static pointer ff_regex_quote(scheme *sc, pointer args) {
	if (!is_pair(args) || !is_string(pair_car(args))) {
		return sc->F;
	}
	const char *in = string_value(pair_car(args));
	char *out = malloc(strlen(in) * 2 + 1);
	if (!out) {
		return sc->F;
	}
	char *o = out;
	for (; *in; in++) {
		if (strchr(".*+?()[]{}|^$\\", *in)) {
			*o++ = '\\';
		}
		*o++ = *in;
	}
	*o = '\0';
	pointer result = mk_string(sc, out);
	free(out);
	return result;
}

static void load(scheme *sc, const char *format, const char *a, const char *b) {
	char line[256];
	snprintf(line, sizeof(line), format, a, b);
	scheme_load_string(sc, line);
}

static void define_vocabulary(scheme *sc) {
	scheme_define(sc, sc->global_env, intern(sc, "%rule"), mk_foreign_func(sc, ff_rule));
	scheme_define(sc, sc->global_env, intern(sc, "param"), mk_foreign_func(sc, ff_param));
	scheme_define(sc, sc->global_env, intern(sc, "regex-quote"), mk_foreign_func(sc, ff_regex_quote));

	scheme_load_string(sc,
		"(define (allow . args) (%rule #t args))"
		"(define (deny . args) (%rule #f args))"
		"(define (version . args) #t)"
		// what an unknown word, or a form headed by one, evaluates to
		"(define %unknown-value (cons '%unknown '()))");

	for (size_t i = 0; i < sizeof(operations) / sizeof(*operations); i++) {
		load(sc, "(define %s (cons '%%op '%s))", operations[i], operations[i]);
	}
	for (size_t i = 0; i < sizeof(evaluated_filters) / sizeof(*evaluated_filters); i++) {
		load(sc, "(define (%s . args) (cons '%%filter (cons '%s args)))", evaluated_filters[i], evaluated_filters[i]);
	}
	for (size_t i = 0; i < sizeof(quoted_filters) / sizeof(*quoted_filters); i++) {
		const char *name = quoted_filters[i];
		if (strcmp(name, "with") == 0) {
			scheme_load_string(sc, "(macro (with form) `(quote (%modifier ,@(cdr form))))");
		} else {
			load(sc, "(macro (%s form) `(quote (%%filter %s ,@(cdr form))))", name, name);
		}
	}
}

// Profiles write regular expressions as #"...", taken literally. Scheme has no such syntax, so turn
// each into an ordinary string with its backslashes doubled. Returns a malloc'd copy, and whether
// the parentheses balance (the interpreter silently drops an unfinished form at the end).
static char *convert_regex_literals(const char *profile, bool *balanced) {
	char *converted = malloc(strlen(profile) * 2 + 1);
	if (!converted) {
		return NULL;
	}
	char *o = converted;
	long depth = 0;
	bool closed_too_many = false;
	enum { code, comment, string, regex } where = code;

	for (const char *p = profile; *p; p++) {
		if (where == code && *p == '#' && p[1] == '"') {
			where = regex;
			p++; // skip the '#'; the '"' is copied below
		} else if (where == code && *p == ';') {
			where = comment;
		} else if (where == code && *p == '"') {
			where = string;
		} else if (where == comment && *p == '\n') {
			where = code;
		} else if (where == string && *p == '\\' && p[1]) {
			*o++ = *p++; // an escaped character cannot end the string
		} else if (where == regex && *p == '\\') {
			*o++ = '\\';
		} else if ((where == string || where == regex) && *p == '"') {
			where = code;
		} else if (where == code && *p == '(') {
			depth++;
		} else if (where == code && *p == ')' && --depth < 0) {
			closed_too_many = true;
		}
		*o++ = *p;
	}
	*o = '\0';
	*balanced = depth == 0 && !closed_too_many && where != string && where != regex;
	return converted;
}

// one evaluation; returns 0 on success, -1 on error with the interpreter's message in `message`
static int evaluate_once(const char *profile, struct state *state, char *const *unknown_words, size_t unknown_word_count, char *message) {
	// Not scheme_init_new(): that mallocs the interpreter without zeroing it, and scheme_init() leaves
	// some fields untouched, so a second interpreter in the same process inherits stale values from
	// the first's freed memory (seen as a crash in the second pass, depending on heap layout).
	scheme *sc = calloc(1, sizeof(*sc));
	if (!sc || !scheme_init(sc)) {
		free(sc);
		snprintf(message, ERROR_BUFFER_SIZE, "out of memory");
		return -1;
	}

	scheme_set_external_data(sc, state);
	memset(message, 0, ERROR_BUFFER_SIZE);
	scheme_set_output_port_string(sc, message, message + ERROR_BUFFER_SIZE - 1);

	scheme_load_string(sc, tinyscheme_init_scm);
	define_vocabulary(sc);
	for (size_t i = 0; i < unknown_word_count; i++) {
		// a macro, so that a form headed by the word is not evaluated at all: rules written inside
		// something we do not understand must not take effect without it
		load(sc, "(macro (%s form) '%%unknown-value)%s", unknown_words[i], "");
	}

	int result = 0;
	if (sc->retcode != 0 || message[0] != '\0') {
		// the vocabulary itself failed to load; this is a bug here, not in the profile
		result = -1;
	} else {
		scheme_load_string(sc, profile);
		if (sc->retcode != 0 || message[0] != '\0') {
			result = -1;
		}
	}

	scheme_deinit(sc);
	free(sc);
	return result;
}

int sbx_profile_evaluate(const char *profile, const char *const params[], struct sbx_profile *out, char **error) {
	char message[ERROR_BUFFER_SIZE];
	char *unknown_words[MAX_UNKNOWN_WORDS];
	size_t unknown_word_count = 0;
	int result = -1;
	static const char unbound[] = "unbound variable: ";

	out->rules = NULL;
	out->count = 0;

	bool balanced = false;
	char *converted = convert_regex_literals(profile, &balanced);
	if (!converted) {
		snprintf(message, sizeof(message), "out of memory");
		goto out;
	}
	if (!balanced) {
		snprintf(message, sizeof(message), "unbalanced parentheses or unfinished string");
		goto out;
	}

	// A word the profile uses that is not defined stops the interpreter. Define it as "unknown" and
	// start over, so the rest of the profile is still read; rules that use it are left out.
	// The passes that find the unknown words are quiet; one more pass at the end reports what it left out.
	for (bool final = false;;) {
		struct state state = { .params = params, .out = out, .quiet = !final };

		sbx_profile_free(out);
		result = evaluate_once(converted, &state, unknown_words, unknown_word_count, message);
		if (state.out_of_memory) {
			snprintf(message, sizeof(message), "out of memory");
			result = -1;
			break;
		}
		if (result == 0) {
			if (final || unknown_word_count == 0) {
				break;
			}
			final = true;
			continue;
		}

		char *name = strstr(message, unbound);
		if (final || !name || unknown_word_count == MAX_UNKNOWN_WORDS) {
			break;
		}
		name += sizeof(unbound) - 1;
		name[strcspn(name, " \n")] = '\0';
		if (name[0] == '\0' || !(unknown_words[unknown_word_count] = strdup(name))) {
			break;
		}
		fprintf(stderr, "DARLING sandbox profile: unknown word \"%s\"; rules that use it are left out\n", name);
		unknown_word_count++;
	}

out:
	for (size_t i = 0; i < unknown_word_count; i++) {
		free(unknown_words[i]);
	}
	free(converted);

	if (result != 0) {
		sbx_profile_free(out);
		if (error) {
			message[strcspn(message, "\n")] = '\0';
			*error = strdup(message[0] ? message : "profile could not be evaluated");
		}
	}
	return result;
}

void sbx_profile_free(struct sbx_profile *profile) {
	for (size_t i = 0; i < profile->count; i++) {
		free(profile->rules[i].operation);
		free(profile->rules[i].filter);
		free(profile->rules[i].value);
	}
	free(profile->rules);
	profile->rules = NULL;
	profile->count = 0;
}
