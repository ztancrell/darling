// A thread catches its own bad access with a handler that receives and returns the register state
// (EXCEPTION_STATE_IDENTITY, as crash reporters use). The handler points rip at landing().
// Pass: "PASS". Before the fix darlingserver aborts in thread_getstatus_to_user.
#include <mach/mach.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(push, 4)
typedef struct {
	mach_msg_header_t Head;
	mach_msg_body_t msgh_body;
	mach_msg_port_descriptor_t thread;
	mach_msg_port_descriptor_t task;
	NDR_record_t NDR;
	exception_type_t exception;
	mach_msg_type_number_t codeCnt;
	int64_t code[2];
	int flavor;
	mach_msg_type_number_t old_stateCnt;
	natural_t old_state[614];
	mach_msg_trailer_t trailer;
} request_t;
typedef struct {
	mach_msg_header_t Head;
	NDR_record_t NDR;
	kern_return_t RetCode;
	int flavor;
	mach_msg_type_number_t new_stateCnt;
	natural_t new_state[614];
} reply_t;
#pragma pack(pop)

static mach_port_t port;

static void landing(void)
{
	puts("PASS: fault caught, registers received, thread resumed at the address the handler set");
	exit(0);
}

static void *handler(void *arg)
{
	static request_t req;
	static reply_t rep;
	kern_return_t kr = mach_msg(&req.Head, MACH_RCV_MSG, 0, sizeof(req), port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	if (kr != KERN_SUCCESS) { printf("FAIL: receive 0x%x\n", kr); exit(1); }
	x86_thread_state64_t st;
	if (req.flavor != x86_THREAD_STATE64 || req.old_stateCnt != x86_THREAD_STATE64_COUNT) {
		printf("FAIL: flavor %d count %u\n", req.flavor, req.old_stateCnt); exit(1);
	}
	memcpy(&st, req.old_state, sizeof(st));
	printf("handler: exception %d, code 0x%llx, address 0x%llx, rip 0x%llx\n", req.exception,
		(long long)req.code[0], (long long)req.code[1], (unsigned long long)st.__rip);
	if (st.__rip == 0 || req.code[1] != 0x10) { puts("FAIL: state or fault address wrong"); exit(1); }
	st.__rip = (uint64_t)landing;
	st.__rsp = (st.__rsp & ~0xfULL) - 8;

	rep.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(req.Head.msgh_bits), 0);
	rep.Head.msgh_remote_port = req.Head.msgh_remote_port;
	rep.Head.msgh_local_port = MACH_PORT_NULL;
	rep.Head.msgh_id = req.Head.msgh_id + 100;
	rep.NDR = NDR_record;
	rep.RetCode = KERN_SUCCESS;
	rep.flavor = x86_THREAD_STATE64;
	rep.new_stateCnt = x86_THREAD_STATE64_COUNT;
	memcpy(rep.new_state, &st, sizeof(st));
	rep.Head.msgh_size = (mach_msg_size_t)(sizeof(rep) - sizeof(rep.new_state) + sizeof(st));
	kr = mach_msg(&rep.Head, MACH_SEND_MSG, rep.Head.msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	if (kr != KERN_SUCCESS) { printf("FAIL: reply 0x%x\n", kr); exit(1); }
	return NULL;
}

int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	mach_port_t self = mach_task_self();
	pthread_t t;
	if (mach_port_allocate(self, MACH_PORT_RIGHT_RECEIVE, &port) ||
	    mach_port_insert_right(self, port, port, MACH_MSG_TYPE_MAKE_SEND)) { puts("FAIL: port"); return 1; }
	kern_return_t kr = thread_set_exception_ports(mach_thread_self(), EXC_MASK_BAD_ACCESS, port,
		EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES, x86_THREAD_STATE64);
	if (kr != KERN_SUCCESS) { printf("FAIL: thread_set_exception_ports 0x%x\n", kr); return 1; }
	pthread_create(&t, NULL, handler, NULL);
	*(volatile int *)0x10 = 1;
	puts("FAIL: fault did not happen or was skipped");
	return 1;
}
