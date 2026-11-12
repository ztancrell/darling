// Does CoreFoundation pair a custom allocator's allocate and deallocate calls? Mimics Blink's
// StringImpl::CreateCFString: a no-copy CFString made with an allocator whose first allocation takes a
// reference on the backing string and whose deallocate drops it. A reference dropped twice is a
// double free in the renderer. Prints one line per case; exit 0 when every case balances.
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>

static long *current;            // the backing "string": a reference count
static int allocs, deallocs, reallocs, bad;

static void *alloc_cb(CFIndex size, CFOptionFlags hint, void *info) {
	long **header = malloc(sizeof(long *) + size);
	*header = current;
	if (current) { ++*current; current = NULL; }
	allocs++;
	return header + 1;
}
static void dealloc_cb(void *ptr, void *info) {
	long **header = (long **)ptr - 1;
	if (*header) --**header;
	deallocs++;
	*header = (long *)0xdead;   // a second deallocate of the same block then crashes here, visibly
	free(header);
}
static void *realloc_cb(void *ptr, CFIndex size, CFOptionFlags hint, void *info) {
	reallocs++;
	long **header = realloc((long **)ptr - 1, sizeof(long *) + size);
	return header + 1;
}
static CFIndex pref_cb(CFIndex size, CFOptionFlags hint, void *info) { return size; }

static CFAllocatorRef wrapper;
static CFStringRef make(long *ref, int wide, const char *text) {
	static UniChar w[64];
	CFIndex n = strlen(text);
	current = ref;
	CFStringRef s;
	if (wide) {
		for (CFIndex i = 0; i < n; i++) w[i] = text[i];
		s = CFStringCreateWithCharactersNoCopy(wrapper, w, n, kCFAllocatorNull);
	} else
		s = CFStringCreateWithBytesNoCopy(wrapper, (const UInt8 *)text, n, kCFStringEncodingISOLatin1, false, kCFAllocatorNull);
	current = NULL;
	return s;
}
static void check(const char *what, long ref) {
	int ok = ref == 1 && allocs == deallocs;
	if (!ok) bad++;
	fprintf(stderr, "%-34s %s  backing refs=%ld (want 1) allocs=%d deallocs=%d reallocs=%d\n",
	        what, ok ? "ok  " : "FAIL", ref, allocs, deallocs, reallocs);
	allocs = deallocs = reallocs = 0;
}

int main(void) {
	CFAllocatorContext ctx = {0, NULL, NULL, NULL, NULL, alloc_cb, realloc_cb, dealloc_cb, pref_cb};
	wrapper = CFAllocatorCreate(NULL, &ctx);
	const char *texts[] = {"a", "Arial", "Helvetica Neue", "a string longer than any inline or tagged form"};
	for (int wide = 0; wide < 2; wide++) for (int t = 0; t < 4; t++) {
		char what[80]; long ref;
		#define CASE(name, body) do { ref = 1; CFStringRef s = make(&ref, wide, texts[t]); \
			if (s) { body; CFRelease(s); } snprintf(what, sizeof what, "%s %s len=%zu", wide ? "utf16" : "latin1", name, strlen(texts[t])); check(what, ref); } while (0)
		CASE("release", (void)0);
		CASE("retain", CFRetain(s); CFRelease(s));
		CASE("copy", CFStringRef c = CFStringCreateCopy(NULL, s); CFRelease(c));
		CASE("copy-same-alloc", CFStringRef c = CFStringCreateCopy(wrapper, s); CFRelease(c));
		CASE("mutable-copy", CFMutableStringRef c = CFStringCreateMutableCopy(NULL, 0, s); CFStringAppend(c, s); CFRelease(c));
		CASE("hash-equal", CFHash(s); CFEqual(s, CFSTR("Arial")));
		CASE("dict-key", CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks); CFDictionarySetValue(d, s, s); CFRelease(d));
		CASE("attributed", CFAttributedStringRef a = CFAttributedStringCreate(NULL, s, NULL); CFRelease(a));
		CASE("cstring", char b[256]; CFStringGetCString(s, b, sizeof b, kCFStringEncodingUTF8); CFStringGetCStringPtr(s, kCFStringEncodingUTF8));
		CASE("url", CFURLRef u = CFURLCreateWithString(NULL, s, NULL); if (u) CFRelease(u));
	}
	fprintf(stderr, "%s\n", bad ? "FAIL" : "PASS");
	return bad != 0;
}
