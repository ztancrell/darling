// Font tables through CoreGraphics: tag list and the 'head' table (always 54 bytes, magic 0x5F0F3CF5 at 12).
// Usage: fonttabletest [font name]
#include <CoreGraphics/CoreGraphics.h>
#include <stdio.h>
int main(int argc, char** argv) {
	CFStringRef name = CFStringCreateWithCString(NULL, argc > 1 ? argv[1] : "Helvetica", kCFStringEncodingUTF8);
	CGFontRef font = CGFontCreateWithFontName(name);
	if (!font) { printf("FAIL no font\n"); return 1; }
	CFArrayRef tags = CGFontCopyTableTags(font);
	long n = tags ? CFArrayGetCount(tags) : -1; int hasHead = 0;
	for (long i = 0; i < n; i++) if ((uintptr_t)CFArrayGetValueAtIndex(tags, i) == 'head') hasHead = 1;
	CFDataRef head = CGFontCopyTableForTag(font, 'head');
	long len = head ? CFDataGetLength(head) : -1; const UInt8* b = head ? CFDataGetBytePtr(head) : NULL;
	int magic = len >= 16 && b[12] == 0x5F && b[13] == 0x0F && b[14] == 0x3C && b[15] == 0xF5;
	CFDataRef none = CGFontCopyTableForTag(font, 'zzzz');
	printf("tags %ld, head listed %d, head length %ld, magic %d, absent table %s\n", n, hasHead, len, magic, none ? "NOT NULL" : "NULL");
	int ok = n > 5 && hasHead && len == 54 && magic && !none;
	printf(ok ? "PASS\n" : "FAIL\n"); return !ok;
}
