// How long the waits a message loop is built from really take, asked for 16 ms each, 60 times:
// usleep, kevent EVFILT_TIMER, kevent64 with a timeout, mach_msg receive timeout, pthread_cond_timedwait,
// a dispatch timer and a CFRunLoop timer. Each line: average and worst wait in milliseconds.
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <sys/event.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#define N 60
#define MS 16
static double now(void) { static mach_timebase_info_data_t tb; if (!tb.denom) mach_timebase_info(&tb); return mach_absolute_time() * (double)tb.numer / tb.denom / 1e6; }
static int bad;
static void report(const char* name, double total, double worst) {
	int ok = total / N < MS * 1.5 && worst < MS * 4; if (!ok) bad = 1;
	printf("%s %-28s average %6.1f ms, worst %6.1f ms\n", ok ? "ok  " : "SLOW", name, total / N, worst);
}
#define LOOP(name, ...) do { double total = 0, worst = 0; for (int i = 0; i < N; i++) { double t = now(); __VA_ARGS__; t = now() - t; total += t; if (t > worst) worst = t; } report(name, total, worst); } while (0)
static int fired; static double last, dtotal, dworst;
static void tick(void) { double t = now(); double d = t - last; last = t; dtotal += d; if (d > dworst) dworst = d; fired++; }
static void cftick(CFRunLoopTimerRef timer, void* info) { tick(); if (fired >= N) CFRunLoopStop(CFRunLoopGetCurrent()); }
static CFRunLoopTimerRef other_timer; static CFRunLoopRef main_loop; static int from_thread;
static void* arm_from_thread(void* a) { CFRunLoopTimerSetNextFireDate(other_timer, CFAbsoluteTimeGetCurrent() + MS / 1000.0); return 0; }
static void cfrearm(CFRunLoopTimerRef timer, void* info) {
	tick();
	if (fired >= N) { CFRunLoopStop(CFRunLoopGetCurrent()); return; }
	if (from_thread) { pthread_t th; pthread_create(&th, 0, arm_from_thread, 0); pthread_detach(th); }
	else CFRunLoopTimerSetNextFireDate(timer, CFAbsoluteTimeGetCurrent() + MS / 1000.0);
}
int main(void) {
	setvbuf(stdout, NULL, _IONBF, 0);
	LOOP("usleep", usleep(MS * 1000));
	int kq = kqueue(); struct kevent ev, out;
	LOOP("kevent EVFILT_TIMER oneshot", EV_SET(&ev, 1, EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0, MS, 0); kevent(kq, &ev, 1, &out, 1, NULL));
	struct timespec ts = { 0, MS * 1000000 }; struct kevent64_s out64;
	LOOP("kevent64 timeout", kevent64(kq, NULL, 0, &out64, 1, 0, &ts));
	mach_port_t port; mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port); struct { mach_msg_header_t h; char pad[64]; } msg;
	LOOP("mach_msg receive timeout", mach_msg(&msg.h, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof msg, port, MS, MACH_PORT_NULL));
	pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER; pthread_cond_t c = PTHREAD_COND_INITIALIZER; pthread_mutex_lock(&m);
	LOOP("pthread_cond_timedwait", struct timeval tv; gettimeofday(&tv, 0); struct timespec at = { tv.tv_sec, (tv.tv_usec + MS * 1000) * 1000 }; if (at.tv_nsec >= 1000000000) { at.tv_sec++; at.tv_nsec -= 1000000000; } pthread_cond_timedwait(&c, &m, &at));
	dispatch_semaphore_t sem = dispatch_semaphore_create(0);
	dispatch_source_t src = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_global_queue(0, 0));
	dispatch_source_set_timer(src, dispatch_time(DISPATCH_TIME_NOW, MS * NSEC_PER_MSEC), MS * NSEC_PER_MSEC, 0);
	dispatch_source_set_event_handler(src, ^{ tick(); if (fired == N) dispatch_semaphore_signal(sem); });
	fired = 0; dtotal = dworst = 0; last = now(); dispatch_resume(src); dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER); dispatch_source_cancel(src);
	report("dispatch timer source", dtotal, dworst);
	fired = 0; dtotal = dworst = 0; last = now();
	CFRunLoopTimerRef timer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + MS / 1000.0, MS / 1000.0, 0, 0, cftick, NULL);
	CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopCommonModes); CFRunLoopRun();
	report("CFRunLoop timer", dtotal, dworst);
	// Chromium's way: one timer that never fires by itself, re-armed for the next deadline with
	// CFRunLoopTimerSetNextFireDate each time it runs.
	fired = 0; dtotal = dworst = 0; last = now();
	CFRunLoopTimerRef rearmed = CFRunLoopTimerCreate(NULL, 1e10, 1e10, 0, 0, cfrearm, NULL);
	CFRunLoopAddTimer(CFRunLoopGetCurrent(), rearmed, kCFRunLoopCommonModes);
	CFRunLoopTimerSetNextFireDate(rearmed, CFAbsoluteTimeGetCurrent() + MS / 1000.0); CFRunLoopRun();
	report("CFRunLoop timer, re-armed", dtotal, dworst);
	// The same, but armed from another thread while the loop sleeps (a task posted to a waiting loop).
	fired = 0; dtotal = dworst = 0; last = now(); other_timer = rearmed; main_loop = CFRunLoopGetCurrent(); from_thread = 1;
	pthread_t th; pthread_create(&th, 0, arm_from_thread, 0); CFRunLoopRun();
	report("re-armed from another thread", dtotal, dworst);
	printf(bad ? "RESULT: SLOW\n" : "RESULT: PASS\n"); return bad;
}
