// The generic atomic helpers every Mac has in libSystem (compiler-rt's atomic.c): __atomic_load,
// __atomic_store, __atomic_exchange, __atomic_compare_exchange. Looked up by name, as a program that
// resolves its imports at run time does, then used on an object too large for a lock-free instruction.
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
struct big { char bytes[88]; };
int main(void) {
	int bad = 0; setvbuf(stdout, 0, _IONBF, 0);
	void (*load)(size_t, void*, void*, int) = dlsym(RTLD_DEFAULT, "__atomic_load");
	void (*store)(size_t, void*, void*, int) = dlsym(RTLD_DEFAULT, "__atomic_store");
	void (*xchg)(size_t, void*, void*, void*, int) = dlsym(RTLD_DEFAULT, "__atomic_exchange");
	int (*cas)(size_t, void*, void*, void*, int, int) = dlsym(RTLD_DEFAULT, "__atomic_compare_exchange");
	printf("%s lookups: load=%p store=%p exchange=%p compare_exchange=%p\n", load && store && xchg && cas ? "ok  " : "FAIL", (void*)load, (void*)store, (void*)xchg, (void*)cas);
	if (!(load && store && xchg && cas)) { printf("RESULT: FAIL\n"); return 1; }
	struct big a, b, c, old; memset(&a, 0, sizeof a); memset(&b, 'B', sizeof b); memset(&c, 'C', sizeof c);
	store(sizeof a, &a, &b, 5); load(sizeof a, &a, &old, 5);
	int ok1 = !memcmp(&old, &b, sizeof b); printf("%s store then load of 88 bytes\n", ok1 ? "ok  " : "FAIL"); bad |= !ok1;
	xchg(sizeof a, &a, &c, &old, 5);
	int ok2 = !memcmp(&old, &b, sizeof b) && !memcmp(&a, &c, sizeof c); printf("%s exchange returns the old value and stores the new\n", ok2 ? "ok  " : "FAIL"); bad |= !ok2;
	struct big expect = b; int r1 = cas(sizeof a, &a, &expect, &b, 5, 5);            // a is C, expect B: must fail and set expect to C
	int ok3 = r1 == 0 && !memcmp(&expect, &c, sizeof c); int r2 = cas(sizeof a, &a, &expect, &b, 5, 5);   // now succeeds
	ok3 = ok3 && r2 != 0 && !memcmp(&a, &b, sizeof b); printf("%s compare_exchange fails with the current value, then succeeds\n", ok3 ? "ok  " : "FAIL"); bad |= !ok3;
	printf(bad ? "RESULT: FAIL\n" : "RESULT: PASS\n"); return bad;
}
