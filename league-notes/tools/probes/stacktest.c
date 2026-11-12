// What Darling's pthread reports about thread stacks, against where the stacks really are.
// Blink (WTF::MayNotBeMainThread) decides "this is the main thread" from stack addresses alone:
// main's pthread_get_stackaddr_np minus an assumed size. A thread whose stack falls inside that
// window is given the main thread's per-thread data.
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/resource.h>
#define THREADS 8
static uintptr_t main_start; static size_t main_size;
static void report(const char* who) {
	uintptr_t local; pthread_t self = pthread_self();
	uintptr_t start = (uintptr_t)pthread_get_stackaddr_np(self); size_t size = pthread_get_stacksize_np(self);
	uintptr_t sp = (uintptr_t)&local;
	int inside_own = sp < start && sp >= start - size;
	uintptr_t diff = main_start - sp;
	printf("%-8s main_np=%d stackaddr=%#lx size=%#zx sp=%#lx own_stack_contains_sp=%d", who, pthread_main_np(), start, size, sp, inside_own);
	if (main_start) printf(" main_start-sp=%#lx within_main_reported=%d within_8MB=%d", diff, diff < main_size, diff < 8u * 1024 * 1024);
	printf("\n");
}
static void* run(void* arg) { char name[16]; snprintf(name, sizeof name, "thread%ld", (long)arg); report(name); return NULL; }
int main(void) {
	struct rlimit rl; getrlimit(RLIMIT_STACK, &rl);
	printf("RLIMIT_STACK cur=%#llx max=%#llx\n", (unsigned long long)rl.rlim_cur, (unsigned long long)rl.rlim_max);
	report("main");
	main_start = (uintptr_t)pthread_get_stackaddr_np(pthread_self()); main_size = pthread_get_stacksize_np(pthread_self());
	report("main");
	pthread_t t[THREADS];
	for (long i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, run, (void*)i);
	for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
	return 0;
}
