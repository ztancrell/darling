// Name resolution as an app sees it: getaddrinfo for a few names, with timing.
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
static double now(void) { struct timeval t; gettimeofday(&t, 0); return t.tv_sec + t.tv_usec / 1e6; }
int main(int argc, char** argv) {
	const char* def[] = { "localhost", "example.com", "data.riotgames.com", "clientconfig.rpg.riotgames.com", "auth.riotgames.com" };
	int n = argc > 1 ? argc - 1 : 5; const char** names = argc > 1 ? (const char**)argv + 1 : def;
	for (int i = 0; i < n; i++) {
		struct addrinfo hints = { .ai_socktype = SOCK_STREAM }, *res = 0; double t = now();
		int rc = getaddrinfo(names[i], "443", &hints, &res);
		char buf[64] = "-";
		if (!rc && res) { void* a = res->ai_family == AF_INET ? (void*)&((struct sockaddr_in*)res->ai_addr)->sin_addr : (void*)&((struct sockaddr_in6*)res->ai_addr)->sin6_addr; inet_ntop(res->ai_family, a, buf, sizeof buf); }
		printf("%-34s rc=%d (%s) first=%s  %.2fs\n", names[i], rc, rc ? gai_strerror(rc) : "ok", buf, now() - t);
	}
	return 0;
}
