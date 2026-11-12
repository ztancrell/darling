// Font traits through CoreText: a regular font is neither bold nor italic and has weight 0; Courier is monospaced.
#include <CoreText/CoreText.h>
#include <stdio.h>
static int show(const char* name, CTFontSymbolicTraits must, CTFontSymbolicTraits mustNot) {
	CFStringRef n = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
	CTFontRef f = CTFontCreateWithName(n, 12, NULL); if (!f) { printf("FAIL %s: no font\n", name); return 1; }
	CTFontSymbolicTraits s = CTFontGetSymbolicTraits(f); CFDictionaryRef d = CTFontCopyTraits(f);
	double w = 99, sl = 99; unsigned ds = ~0u;
	if (d) {
		CFNumberRef x;
		if ((x = CFDictionaryGetValue(d, kCTFontWeightTrait))) CFNumberGetValue(x, kCFNumberDoubleType, &w);
		if ((x = CFDictionaryGetValue(d, kCTFontSlantTrait))) CFNumberGetValue(x, kCFNumberDoubleType, &sl);
		if ((x = CFDictionaryGetValue(d, kCTFontSymbolicTrait))) CFNumberGetValue(x, kCFNumberSInt32Type, &ds);
	}
	int ok = d && ds == s && (s & must) == must && !(s & mustNot) && w >= -1 && w <= 1 && sl >= -1 && sl <= 1;
	printf("%s %s: symbolic 0x%x, dictionary %s, weight %.2f, slant %.2f\n", ok ? "ok  " : "FAIL", name, (unsigned)s, d ? "yes" : "NULL", w, sl);
	return !ok;
}
// The box of 'H' at 100 points: written to the array, about 72 points tall (the cap height), and twice that at 200.
static int boxes(void) {
	UniChar ch[2] = { 'H', ' ' }; CGGlyph g[2] = { 0, 0 }; CGRect r[2] = { { { -1, -1 }, { -1, -1 } }, { { -1, -1 }, { -1, -1 } } }, r2[2];
	CTFontRef f = CTFontCreateWithName(CFSTR("Helvetica"), 100, NULL), f2 = CTFontCreateWithName(CFSTR("Helvetica"), 200, NULL);
	if (!CTFontGetGlyphsForCharacters(f, ch, g, 2)) { printf("FAIL boxes: no glyphs\n"); return 1; }
	CGRect all = CTFontGetBoundingRectsForGlyphs(f, kCTFontOrientationHorizontal, g, r, 2);
	CTFontGetBoundingRectsForGlyphs(f2, kCTFontOrientationHorizontal, g, r2, 2);
	int ok = r[0].size.height > 55 && r[0].size.height < 90 && r[0].size.width > 30 && r[0].size.width < 110 &&
		r[1].size.width == 0 && r[1].size.height == 0 && all.size.height == r[0].size.height &&
		r2[0].size.height > 1.9 * r[0].size.height && r2[0].size.height < 2.1 * r[0].size.height && CTFontGetUnitsPerEm(f) >= 1000;
	printf("%s boxes: H %.1fx%.1f at (%.1f,%.1f), space %.1fx%.1f, H at 200pt height %.1f, units per em %u\n", ok ? "ok  " : "FAIL",
		r[0].size.width, r[0].size.height, r[0].origin.x, r[0].origin.y, r[1].size.width, r[1].size.height, r2[0].size.height, CTFontGetUnitsPerEm(f));
	return !ok;
}
int main(void) {
	int bad = show("Helvetica", 0, kCTFontTraitItalic | kCTFontTraitBold | kCTFontTraitMonoSpace) | show("Courier", kCTFontTraitMonoSpace, kCTFontTraitItalic) | boxes();
	printf(bad ? "RESULT: FAIL\n" : "RESULT: PASS\n"); return bad;
}
