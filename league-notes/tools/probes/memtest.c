// Mach shared memory check: make a new memory entry, map it twice, and see one mapping's writes in the other.
// Also checks that the unimplemented flavour (entry for existing memory) fails with an error and leaves the server alive.
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <stdio.h>
#include <string.h>

static int failed;
#define CHECK(cond, what) do { int ok_ = (cond); printf("%s %s\n", ok_ ? "ok  " : "FAIL", what); if (!ok_) failed = 1; } while (0)

int main(void) {
	mach_port_t self = mach_task_self(), h = MACH_PORT_NULL, h2 = MACH_PORT_NULL, p = MACH_PORT_NULL;
	memory_object_size_t size = 5000, size2 = 4096;
	mach_vm_address_t a = 0, b = 0, c = 0, d = 0;
	kern_return_t kr;
	setvbuf(stdout, NULL, _IONBF, 0); // keep the lines when a failed check aborts the process

	kr = mach_make_memory_entry_64(self, &size, 0, MAP_MEM_NAMED_CREATE | VM_PROT_READ | VM_PROT_WRITE, &h, MACH_PORT_NULL);
	printf("create: kr=%d size=%llu handle=0x%x\n", kr, size, h);
	CHECK(kr == KERN_SUCCESS && h != MACH_PORT_NULL && size == 8192, "new entry, size rounded to pages");

	kr = mach_vm_map(self, &a, size, 0, VM_FLAGS_ANYWHERE, h, 0, FALSE, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
	printf("map rw: kr=%d addr=0x%llx\n", kr, a);
	CHECK(kr == KERN_SUCCESS && a != 0, "first mapping");
	kr = mach_vm_map(self, &b, 4096, 0, VM_FLAGS_ANYWHERE, h, 4096, FALSE, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
	printf("map ro at offset 4096: kr=%d addr=0x%llx\n", kr, b);
	CHECK(kr == KERN_SUCCESS && b != 0 && b != a, "second mapping, second page only");
	if (failed) return 1; // the writes below would fault

	strcpy((char*)a + 4096, "shared");
	CHECK(strcmp((char*)b, "shared") == 0, "write through one mapping is visible through the other");

	kr = mach_vm_map(self, &d, 4096, 0, VM_FLAGS_ANYWHERE, h, 8192, FALSE, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
	CHECK(kr != KERN_SUCCESS, "mapping past the end of the entry is refused");

	kr = mach_port_deallocate(self, h);
	CHECK(kr == KERN_SUCCESS && strcmp((char*)b, "shared") == 0, "mappings outlive the handle");

	kr = mach_vm_map(self, &c, 8192, 0, VM_FLAGS_ANYWHERE, MACH_PORT_NULL, 0, FALSE, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
	printf("map without a handle: kr=%d addr=0x%llx\n", kr, c);
	CHECK(kr == KERN_SUCCESS && c != 0, "mapping without a handle allocates memory");
	if (kr == KERN_SUCCESS && c != 0) { memset((void*)c, 7, 8192); CHECK(((char*)c)[8191] == 7, "that memory is usable"); }

	// live view of memory that already exists (ordinary private memory)
	mach_vm_address_t e = 0, f = 0, g = 0, k = 0;
	mach_port_t h3 = MACH_PORT_NULL, h4 = MACH_PORT_NULL;
	memory_object_size_t size3 = 40, size4 = 4096;
	kr = mach_vm_allocate(self, &e, 3 * 4096, VM_FLAGS_ANYWHERE);
	CHECK(kr == KERN_SUCCESS && e != 0, "private memory allocated");
	if (failed) return 1;
	char* owner = (char*)e + 4096 + 100;
	strcpy(owner, "before");
	strcpy((char*)e, "page0"); strcpy((char*)e + 2 * 4096, "page2");
	kr = mach_make_memory_entry_64(self, &size3, (memory_object_offset_t)(uintptr_t)owner, VM_PROT_READ, &h3, MACH_PORT_NULL);
	printf("entry for 40 bytes of existing memory: kr=%d size=%llu handle=0x%x\n", kr, size3, h3);
	CHECK(kr == KERN_SUCCESS && h3 != MACH_PORT_NULL && size3 == 4096, "read-only entry for existing memory, rounded to its page");
	CHECK(strcmp(owner, "before") == 0 && strcmp((char*)e, "page0") == 0 && strcmp((char*)e + 2 * 4096, "page2") == 0, "owner's memory kept its contents");
	kr = mach_vm_map(self, &f, 4096, 0, VM_FLAGS_ANYWHERE, h3, 0, FALSE, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
	printf("map it: kr=%d addr=0x%llx\n", kr, f);
	CHECK(kr == KERN_SUCCESS && f != 0 && f != e + 4096, "mapped somewhere else");
	if (failed) return 1;
	CHECK(strcmp((char*)f + 100, "before") == 0, "mapping shows the existing contents");
	strcpy(owner, "after");
	CHECK(strcmp((char*)f + 100, "after") == 0, "a later write by the owner shows through the mapping (live, not a snapshot)");
	kr = mach_vm_map(self, &g, 4096, 0, VM_FLAGS_ANYWHERE, h3, 0, FALSE, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
	CHECK(kr != KERN_SUCCESS, "writable mapping of a read-only entry is refused");

	// entry for memory that is already one of our shared regions (second page of `a`)
	kr = mach_make_memory_entry_64(self, &size4, a + 4096, VM_PROT_READ, &h4, MACH_PORT_NULL);
	CHECK(kr == KERN_SUCCESS && h4 != MACH_PORT_NULL, "entry for part of an already shared region");
	if (kr == KERN_SUCCESS) {
		kr = mach_vm_map(self, &k, 4096, 0, VM_FLAGS_ANYWHERE, h4, 0, FALSE, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
		CHECK(kr == KERN_SUCCESS && k != 0 && strcmp((char*)k, "shared") == 0, "and it maps the right page");
	}

	// entry for a range that only partly overlaps a region shared earlier: 4 private pages, share page 1, then
	// ask for pages 0-2 (private, shared, private). It must come out as one entry with a live view of all three.
	{
		mach_vm_address_t m = 0, v1 = 0, v2 = 0; mach_port_t hA = MACH_PORT_NULL, hB = MACH_PORT_NULL;
		memory_object_size_t sA = 4096, sB = 3 * 4096;
		kr = mach_vm_allocate(self, &m, 4 * 4096, VM_FLAGS_ANYWHERE);
		CHECK(kr == KERN_SUCCESS, "overlap: 4 private pages");
		strcpy((char*)m, "p0"); strcpy((char*)m + 4096, "p1"); strcpy((char*)m + 2 * 4096, "p2"); strcpy((char*)m + 3 * 4096, "p3");
		kr = mach_make_memory_entry_64(self, &sA, m + 4096, VM_PROT_READ | VM_PROT_WRITE, &hA, MACH_PORT_NULL);
		CHECK(kr == KERN_SUCCESS, "overlap: entry for page 1");
		kr = mach_vm_map(self, &v1, 4096, 0, VM_FLAGS_ANYWHERE, hA, 0, FALSE, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
		CHECK(kr == KERN_SUCCESS && strcmp((char*)v1, "p1") == 0, "overlap: page 1 mapped");
		kr = mach_make_memory_entry_64(self, &sB, m, VM_PROT_READ | VM_PROT_WRITE, &hB, MACH_PORT_NULL);
		printf("overlap: entry for pages 0-2: kr=%d size=%llu\n", kr, sB);
		CHECK(kr == KERN_SUCCESS && hB != MACH_PORT_NULL && sB == 3 * 4096, "overlap: entry for pages 0-2 (partly shared already)");
		CHECK(strcmp((char*)m, "p0") == 0 && strcmp((char*)m + 4096, "p1") == 0 && strcmp((char*)m + 2 * 4096, "p2") == 0 && strcmp((char*)m + 3 * 4096, "p3") == 0, "overlap: owner's memory kept its contents");
		kr = mach_vm_map(self, &v2, 3 * 4096, 0, VM_FLAGS_ANYWHERE, hB, 0, FALSE, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
		CHECK(kr == KERN_SUCCESS && v2 != 0, "overlap: all three pages mapped at once");
		if (kr == KERN_SUCCESS) {
			CHECK(strcmp((char*)v2, "p0") == 0 && strcmp((char*)v2 + 4096, "p1") == 0 && strcmp((char*)v2 + 2 * 4096, "p2") == 0, "overlap: mapping shows every page in order");
			strcpy((char*)m, "o0"); strcpy((char*)m + 4096, "o1"); strcpy((char*)m + 2 * 4096, "o2");
			CHECK(strcmp((char*)v2, "o0") == 0 && strcmp((char*)v2 + 4096, "o1") == 0 && strcmp((char*)v2 + 2 * 4096, "o2") == 0, "overlap: owner's later writes show through (live)");
			CHECK(strcmp((char*)v1, "o1") == 0, "overlap: the earlier mapping of page 1 is still live");
			strcpy((char*)v1, "w1");
			CHECK(strcmp((char*)v2 + 4096, "w1") == 0 && strcmp((char*)m + 4096, "w1") == 0, "overlap: a write through the earlier mapping reaches the new one and the owner");
			mach_vm_address_t v3 = 0;
			kr = mach_vm_map(self, &v3, 4096, 0, VM_FLAGS_ANYWHERE, hB, 2 * 4096, FALSE, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
			CHECK(kr == KERN_SUCCESS && strcmp((char*)v3, "o2") == 0, "overlap: mapping only the last page of the entry");
		}
	}

	// reused address: map entry A, unmap it, map entry B at the same address. An entry made from that address must
	// be B's memory (the server is not told about the unmap and must not answer from its record of A).
	{
		mach_port_t hA = MACH_PORT_NULL, hB = MACH_PORT_NULL, hR = MACH_PORT_NULL;
		memory_object_size_t sA = 2 * 4096, sB = 4096, sR = 4096; mach_vm_address_t x = 0, y = 0, v = 0;
		kr = mach_make_memory_entry_64(self, &sA, 0, MAP_MEM_NAMED_CREATE | VM_PROT_READ | VM_PROT_WRITE, &hA, MACH_PORT_NULL);
		if (kr == KERN_SUCCESS) kr = mach_make_memory_entry_64(self, &sB, 0, MAP_MEM_NAMED_CREATE | VM_PROT_READ | VM_PROT_WRITE, &hB, MACH_PORT_NULL);
		if (kr == KERN_SUCCESS) kr = mach_vm_map(self, &x, 2 * 4096, 0, VM_FLAGS_ANYWHERE, hA, 0, FALSE, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
		CHECK(kr == KERN_SUCCESS, "reuse: entry A mapped");
		if (kr == KERN_SUCCESS) {
			strcpy((char*)x, "from A");
			y = x;
			kr = mach_vm_deallocate(self, x, 2 * 4096);
			if (kr == KERN_SUCCESS) kr = mach_vm_map(self, &y, 4096, 0, VM_FLAGS_FIXED, hB, 0, FALSE, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
			CHECK(kr == KERN_SUCCESS && y == x, "reuse: entry B mapped at the same address");
			if (kr == KERN_SUCCESS) {
				strcpy((char*)y, "from B");
				kr = mach_make_memory_entry_64(self, &sR, y, VM_PROT_READ, &hR, MACH_PORT_NULL);
				if (kr == KERN_SUCCESS) kr = mach_vm_map(self, &v, 4096, 0, VM_FLAGS_ANYWHERE, hR, 0, FALSE, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
				CHECK(kr == KERN_SUCCESS && strcmp((char*)v, "from B") == 0, "reuse: read-only entry made from that address is B's memory");
				if (kr == KERN_SUCCESS) { strcpy((char*)y, "B again"); CHECK(strcmp((char*)v, "B again") == 0, "reuse: and it is live"); }
			}
		}
	}

	// many pieces: 20 private pages, every odd one shared on its own first, then one entry for all 20
	{
		mach_vm_address_t m = 0, v = 0; mach_port_t hp = MACH_PORT_NULL, hall = MACH_PORT_NULL; int good = 1;
		memory_object_size_t sall = 20 * 4096;
		kr = mach_vm_allocate(self, &m, 20 * 4096, VM_FLAGS_ANYWHERE);
		for (int i = 0; kr == KERN_SUCCESS && i < 20; i++) ((char*)m)[i * 4096] = 'a' + i;
		for (int i = 1; kr == KERN_SUCCESS && i < 20; i += 2) {
			memory_object_size_t one = 4096;
			kr = mach_make_memory_entry_64(self, &one, m + i * 4096, VM_PROT_READ, &hp, MACH_PORT_NULL);
		}
		CHECK(kr == KERN_SUCCESS, "many: ten separate one-page entries");
		kr = mach_make_memory_entry_64(self, &sall, m, VM_PROT_READ | VM_PROT_WRITE, &hall, MACH_PORT_NULL);
		CHECK(kr == KERN_SUCCESS && sall == 20 * 4096, "many: one entry over all 20 pages");
		kr = mach_vm_map(self, &v, 20 * 4096, 0, VM_FLAGS_ANYWHERE, hall, 0, FALSE, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
		CHECK(kr == KERN_SUCCESS, "many: mapped");
		if (kr == KERN_SUCCESS) {
			for (int i = 0; i < 20; i++) if (((char*)v)[i * 4096] != 'a' + i) good = 0;
			CHECK(good, "many: every page in order");
			for (int i = 0; i < 20; i++) ((char*)m)[i * 4096] = 'A' + i;
			for (int i = 0; i < 20; i++) if (((char*)v)[i * 4096] != 'A' + i) good = 0;
			CHECK(good, "many: live on every page");
		}
	}

	kr = mach_make_memory_entry_64(self, &size2, e, MAP_MEM_VM_COPY | VM_PROT_READ, &h2, MACH_PORT_NULL);
	printf("private-copy entry: kr=%d\n", kr);
	CHECK(kr != KERN_SUCCESS, "unimplemented flavour returns an error");

	kr = mach_port_allocate(self, MACH_PORT_RIGHT_RECEIVE, &p);
	CHECK(kr == KERN_SUCCESS, "server still answers afterwards");

	printf(failed ? "RESULT: FAIL\n" : "RESULT: PASS\n");
	return failed;
}
