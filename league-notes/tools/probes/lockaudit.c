// Inserted library (DYLD_INSERT_LIBRARIES) that audits os_unfair_lock use by one image (default: any image
// whose path contains "Electron Framework"). For every lock it remembers who holds it; a second holder
// while the first still holds it is a mutual-exclusion failure and is reported with everything known,
// then the process aborts. Locks taken by other images are passed through untouched.
#include <os/lock.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#define N 65536
static _Atomic uint64_t held[N];           // (low 32 bits of lock address << 32) | holder's thread port
static uintptr_t lo, hi;
static inline uint32_t slot3(void) { uint64_t r; __asm__("movq %%gs:0x18, %0" : "=r"(r)); return (uint32_t)r; }
static void find_image(void) {
	for (uint32_t i = 0; i < _dyld_image_count(); i++) {
		const char* n = _dyld_get_image_name(i);
		if (!n || !strstr(n, "Electron Framework")) continue;
		const struct mach_header_64* mh = (const void*)_dyld_get_image_header(i);
		const struct load_command* lc = (const void*)(mh + 1);
		for (uint32_t c = 0; c < mh->ncmds; c++, lc = (const void*)((const char*)lc + lc->cmdsize)) {
			const struct segment_command_64* sg = (const void*)lc;
			if (lc->cmd == LC_SEGMENT_64 && !strcmp(sg->segname, "__TEXT")) { lo = (uintptr_t)mh; hi = lo + sg->vmsize; }
		}
	}
}
__attribute__((constructor)) static void init(void) { find_image(); }
// Only the constructor looks for the image: dyld takes these same locks, so looking it up from inside a lock call recurses.
static inline int ours(void* ra) { return (uintptr_t)ra >= lo && (uintptr_t)ra < hi; }
static void report(const char* what, os_unfair_lock_t l, uint32_t self, uint64_t prev, void* ra) {
	char b[400];
	int n = snprintf(b, sizeof b, "LOCKAUDIT pid %d: %s lock=%p word=%#x self(tsd3)=%#x pthread_mach_thread_np=%#x previous holder=%#x caller=Electron Framework+%#lx\n",
		getpid(), what, (void*)l, *(volatile uint32_t*)l, self, pthread_mach_thread_np(pthread_self()), (uint32_t)prev, (unsigned long)((uintptr_t)ra - lo));
	write(2, b, n); abort();
}
static inline void took(os_unfair_lock_t l, void* ra) {
	uint32_t self = slot3(); uint64_t tag = (uint64_t)(uint32_t)(uintptr_t)l << 32;
	if (!self) report("thread has no owner tag (slot 3 is 0)", l, self, 0, ra);
	uint64_t prev = atomic_exchange(&held[((uintptr_t)l >> 2) % N], tag | self);
	if ((prev >> 32) == (tag >> 32) && (uint32_t)prev) report("TWO HOLDERS", l, self, prev, ra);
}
static inline void dropping(os_unfair_lock_t l, void* ra) {
	uint32_t self = slot3(); uint64_t tag = (uint64_t)(uint32_t)(uintptr_t)l << 32, mine = tag | self;
	uint32_t word = *(volatile uint32_t*)l;
	if ((word | 1) != (self | 1)) report("unlock by a thread that does not own it", l, self, held[((uintptr_t)l >> 2) % N], ra);
	atomic_compare_exchange_strong(&held[((uintptr_t)l >> 2) % N], &mine, tag);
}

// The real work goes through entry points this library does not interpose (and a plain compare-and-swap
// for trylock, which is all the real one does), so it cannot end up calling itself.
extern void os_unfair_lock_lock_with_options(os_unfair_lock_t, uint32_t);
extern void os_unfair_lock_unlock_no_tsd(os_unfair_lock_t, uint32_t self);
void audit_lock(os_unfair_lock_t l) { void* ra = __builtin_return_address(0); os_unfair_lock_lock_with_options(l, 0); if (ours(ra)) took(l, ra); }
bool audit_trylock(os_unfair_lock_t l) { void* ra = __builtin_return_address(0); bool r = __sync_bool_compare_and_swap((uint32_t*)l, 0, slot3()); if (r && ours(ra)) took(l, ra); return r; }
void audit_unlock(os_unfair_lock_t l) { void* ra = __builtin_return_address(0); if (ours(ra)) dropping(l, ra); os_unfair_lock_unlock_no_tsd(l, slot3()); }
#define INTERPOSE(new, old) __attribute__((used, section("__DATA,__interpose"))) static struct { const void* n; const void* o; } ip_##old = { (const void*)new, (const void*)old }
INTERPOSE(audit_lock, os_unfair_lock_lock);
INTERPOSE(audit_trylock, os_unfair_lock_trylock);
INTERPOSE(audit_unlock, os_unfair_lock_unlock);
