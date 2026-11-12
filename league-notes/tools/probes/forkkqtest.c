// fork() in a process whose other threads keep the kqueue layer's lock busy, with an atfork child handler
// that closes a file (libuv registers one like it). The child must get through its handlers and exit.
// A child that never exits means it inherited a lock held by a thread that does not exist in it.
#include <sys/event.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
static volatile int stop;
static void* churn(void* arg) { while (!stop) close(dup(0)); return 0; }     // close() takes the kqueue layer's lock
static void child_handler(void) { close(dup(0)); }
int main(int argc, char** argv) {
	int forks = 300, hung = 0; setvbuf(stdout, 0, _IONBF, 0);
	int kq = kqueue(); if (kq < 0) { perror("kqueue"); return 2; }
	pthread_t t[4]; for (int i = 0; i < 4; i++) pthread_create(&t[i], 0, churn, 0);
	pthread_atfork(0, 0, child_handler);
	for (int i = 0; i < forks && !hung; i++) {
		pid_t p = fork();
		if (p == 0) _exit(0);
		int st, waited = 0;
		while (waitpid(p, &st, WNOHANG) == 0) { usleep(2000); if (++waited > 1500) { hung = i + 1; kill(p, SIGKILL); waitpid(p, &st, 0); break; } }
	}
	stop = 1; for (int i = 0; i < 4; i++) pthread_join(t[i], 0);
	if (hung) printf("fork %d of %d: child never exited\nRESULT: FAIL\n", hung, forks); else printf("%d forks, every child exited\nRESULT: PASS\n", forks);
	return hung != 0;
}
