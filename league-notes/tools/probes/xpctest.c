// A synchronous XPC request to a service: with a name nobody provides, macOS answers at once with
// "connection invalid". Usage: xpctest <service name> [privileged]
#include <xpc/xpc.h>
#include <dispatch/dispatch.h>
#include <stdio.h>
#include <sys/time.h>
static double now(void) { struct timeval t; gettimeofday(&t, 0); return t.tv_sec + t.tv_usec / 1e6; }
int main(int argc, char** argv) {
	const char* name = argc > 1 ? argv[1] : "com.example.nonexistent";
	xpc_connection_t c = xpc_connection_create_mach_service(name, NULL, argc > 2 ? XPC_CONNECTION_MACH_SERVICE_PRIVILEGED : 0); // any 2nd argument: privileged, as Security asks in a system context
	xpc_connection_set_event_handler(c, ^(xpc_object_t e) {
		printf("event handler: %s\n", xpc_get_type(e) == XPC_TYPE_ERROR ? xpc_dictionary_get_string(e, XPC_ERROR_KEY_DESCRIPTION) : "a message"); fflush(stdout);
	});
	xpc_connection_resume(c);
	xpc_object_t msg = xpc_dictionary_create(NULL, NULL, 0); xpc_dictionary_set_string(msg, "probe", "hello");
	double t = now(); printf("sending to %s and waiting for the reply...\n", name); fflush(stdout);
	xpc_object_t r = xpc_connection_send_message_with_reply_sync(c, msg);
	printf("reply after %.2fs: %s\n", now() - t, xpc_get_type(r) == XPC_TYPE_ERROR ? xpc_dictionary_get_string(r, XPC_ERROR_KEY_DESCRIPTION) : "a dictionary");
	return 0;
}
