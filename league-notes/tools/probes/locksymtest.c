// Which os_unfair_lock_lock/unlock does a program get when it asks libSystem for them by name, and do
// they work? Darling's libsystem_kernel used to export empty ones; a program bound to those never
// really locks (Chromium's allocator was). Checks three ways of asking and that a held lock is non-zero.
#include <os/lock.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
static int check(const char* how, void* h) {
	void (*lock)(os_unfair_lock_t) = dlsym(h, "os_unfair_lock_lock");
	void (*unlock)(os_unfair_lock_t) = dlsym(h, "os_unfair_lock_unlock");
	Dl_info li = {0}, ui = {0}; dladdr(lock, &li); dladdr(unlock, &ui);
	os_unfair_lock l = OS_UNFAIR_LOCK_INIT;
	lock(&l); uint32_t held = *(volatile uint32_t*)&l; if (held) unlock(&l); uint32_t after = *(volatile uint32_t*)&l;
	int ok = held != 0 && after == 0;
	printf("%s %-28s lock from %s, unlock from %s; word while held=%#x, after unlock=%#x\n", ok ? "ok  " : "FAIL", how,
		li.dli_fname ? li.dli_fname : "?", ui.dli_fname ? ui.dli_fname : "?", held, after);
	return !ok;
}
int main(void) {
	int bad = 0;
	bad |= check("dlsym(RTLD_DEFAULT)", RTLD_DEFAULT);
	bad |= check("dlsym(libSystem.B.dylib)", dlopen("/usr/lib/libSystem.B.dylib", RTLD_NOW));
	// libsystem_kernel itself must not offer one at all (RTLD_FIRST: look in that library only)
	void* k = dlopen("/usr/lib/system/libsystem_kernel.dylib", RTLD_NOW | RTLD_FIRST);
	int kernel_has = dlsym(k, "os_unfair_lock_lock") || dlsym(k, "os_unfair_lock_unlock");
	printf("%s libsystem_kernel exports its own lock/unlock: %s\n", kernel_has ? "FAIL" : "ok  ", kernel_has ? "yes" : "no"); bad |= kernel_has;
	os_unfair_lock l = OS_UNFAIR_LOCK_INIT; os_unfair_lock_lock(&l); uint32_t held = *(volatile uint32_t*)&l;
	printf("%s direct call: word while held=%#x\n", held ? "ok  " : "FAIL", held); bad |= !held; if (held) os_unfair_lock_unlock(&l);
	printf(bad ? "RESULT: FAIL\n" : "RESULT: PASS\n"); return bad;
}
