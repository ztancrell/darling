// Per-thread storage the way Chromium reads it: pthread keys read straight from %gs (PartitionAlloc's
// thread cache pointer), pthread_getspecific, and compiler thread-locals. Each thread stores its own
// tag and checks it millions of times while threads come and go. Any mismatch = two threads share a slot.
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define THREADS 8
#define ROUNDS 3000000
static pthread_key_t keys[4];
static __thread uintptr_t tl_a; static __thread uintptr_t tl_b = 7;
static volatile long bad_gs, bad_get, bad_tlv, bad_malloc;
static inline void* gs_get(pthread_key_t k) { void* r; __asm__("movq %%gs:(,%1,8), %0" : "=r"(r) : "r"((uintptr_t)k)); return r; }
static void* run(void* arg) {
	uintptr_t tag = (uintptr_t)arg;
	for (int k = 0; k < 4; k++) pthread_setspecific(keys[k], (void*)(tag * 16 + k));
	tl_a = tag; tl_b = ~tag;
	for (int i = 0; i < ROUNDS; i++) {
		int k = i & 3;
		if (gs_get(keys[k]) != (void*)(tag * 16 + k)) bad_gs++;
		if (pthread_getspecific(keys[k]) != (void*)(tag * 16 + k)) bad_get++;
		if (tl_a != tag || tl_b != ~tag) bad_tlv++;
		if ((i & 1023) == 0) { uintptr_t* p = malloc(24); p[0] = tag; p[2] = i; sched_yield(); if (p[0] != tag || p[2] != (uintptr_t)i) bad_malloc++; free(p); }
	}
	return NULL;
}
int main(void) {
	for (int k = 0; k < 4; k++) pthread_key_create(&keys[k], NULL);
	for (int round = 0; round < 6; round++) {   // threads are created and destroyed repeatedly, as workers are
		pthread_t t[THREADS];
		for (uintptr_t i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, run, (void*)(round * 100 + i + 1));
		for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
	}
	printf("keys %lu %lu %lu %lu | mismatches: gs=%ld getspecific=%ld thread_local=%ld malloc=%ld -> %s\n", (unsigned long)keys[0], (unsigned long)keys[1], (unsigned long)keys[2], (unsigned long)keys[3],
		bad_gs, bad_get, bad_tlv, bad_malloc, bad_gs || bad_get || bad_tlv || bad_malloc ? "FAIL" : "PASS");
	return 0;
}
