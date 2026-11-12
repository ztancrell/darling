// Mutual exclusion of os_unfair_lock the way Chromium's PartitionAlloc uses it: spin on trylock, then
// block in os_unfair_lock_lock (or the private _with_options call when present). N threads add to a
// plain counter under the lock; any lost update means two threads were inside at once.
#include <os/lock.h>
#include <pthread.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#define THREADS 8
#define ROUNDS 400000
static os_unfair_lock lock = OS_UNFAIR_LOCK_INIT;
static volatile long counter; static volatile int inside, overlaps;
static void (*lock_with_options)(os_unfair_lock_t, unsigned);
static int mode;
static void* run(void* arg) {
	for (int i = 0; i < ROUNDS; i++) {
		int got = 0;
		if (mode != 1) for (int s = 0; s < 10 && !got; s++) got = os_unfair_lock_trylock(&lock);
		if (!got) { if (mode == 2 && lock_with_options) lock_with_options(&lock, 0x00010000 | 0x00040000); else os_unfair_lock_lock(&lock); }
		if (__sync_add_and_fetch(&inside, 1) != 1) overlaps++;
		long c = counter; if ((i & 1023) == 0) sched_yield(); counter = c + 1;
		__sync_sub_and_fetch(&inside, 1);
		os_unfair_lock_unlock(&lock);
	}
	return 0;
}
int main(void) {
	int bad = 0; setvbuf(stdout, NULL, _IONBF, 0);
	lock_with_options = dlsym(RTLD_DEFAULT, "os_unfair_lock_lock_with_options");
	const char* names[] = { "trylock spin then lock", "lock only", "trylock spin then lock_with_options" };
	for (mode = 0; mode < 3; mode++) {
		if (mode == 2 && !lock_with_options) { printf("skip %s: symbol absent\n", names[mode]); continue; }
		pthread_t t[THREADS]; counter = 0; overlaps = 0;
		for (int i = 0; i < THREADS; i++) pthread_create(&t[i], 0, run, 0);
		for (int i = 0; i < THREADS; i++) pthread_join(t[i], 0);
		int ok = counter == (long)THREADS * ROUNDS && overlaps == 0;
		printf("%s %s: counter %ld of %ld, overlaps %d\n", ok ? "ok  " : "FAIL", names[mode], counter, (long)THREADS * ROUNDS, overlaps);
		bad |= !ok;
	}
	printf(bad ? "RESULT: FAIL\n" : "RESULT: PASS\n"); return bad;
}
