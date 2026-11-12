// Does os_unfair_lock still lock while a thread is exiting? PartitionAlloc empties a thread's cache
// from a pthread key destructor, under os_unfair_lock. The lock stores the owner's thread port name,
// read from per-thread slot 3 (%gs:0x18). If that slot is already 0 when destructors run, "locking"
// writes 0 into the lock: it stays unlocked and the destructor runs unprotected.
#include <os/lock.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <mach/mach.h>
#define THREADS 6
static pthread_key_t key; static os_unfair_lock lock = OS_UNFAIR_LOCK_INIT;
static volatile int zero_owner, not_held, runs;
static inline uint32_t slot3(void) { uint64_t r; __asm__("movq %%gs:0x18, %0" : "=r"(r)); return (uint32_t)r; }
static void destructor(void* v) {
	uint32_t owner = slot3();
	os_unfair_lock_lock(&lock);
	uint32_t word = *(volatile uint32_t*)&lock;
	os_unfair_lock_unlock(&lock);
	__sync_add_and_fetch(&runs, 1);
	if (!owner) __sync_add_and_fetch(&zero_owner, 1);
	if (!word) __sync_add_and_fetch(&not_held, 1);
	printf("destructor thread %ld: slot3=%#x lock word while held=%#x\n", (long)v, owner, word);
}
static void* run(void* arg) {
	printf("body       thread %ld: slot3=%#x pthread_mach_thread_np=%#x\n", (long)arg, slot3(), pthread_mach_thread_np(pthread_self()));
	pthread_setspecific(key, arg); return NULL;
}
int main(void) {
	setvbuf(stdout, NULL, _IONBF, 0);
	pthread_key_create(&key, destructor);
	pthread_t t[THREADS];
	for (long i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, run, (void*)(i + 1));
	for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
	int bad = zero_owner || not_held || runs != THREADS;
	printf("destructors run %d of %d; owner slot zero in %d; lock not actually held in %d\nRESULT: %s\n", runs, THREADS, zero_owner, not_held, bad ? "FAIL" : "PASS");
	return bad;
}
