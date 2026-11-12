// Darling's exp10 must return, with the right value. It used to tail-call itself forever.
#include <stdio.h>
#include <math.h>
extern double __exp10(double);
extern float __exp10f(float);
int main(void) {
	int failed = 0;
	struct { double x, want; } cases[] = { { 2, 100 }, { 0, 1 }, { -1, 0.1 }, { 38, 1e38 }, { 0.5, 3.1622776601683795 } };
	for (unsigned i = 0; i < sizeof cases / sizeof *cases; i++) {
		double d = __exp10(cases[i].x); float f = __exp10f((float)cases[i].x);
		int ok = fabs(d - cases[i].want) <= cases[i].want * 1e-12 && fabsf(f - (float)cases[i].want) <= (float)cases[i].want * 1e-6f;
		printf("%s exp10(%g) = %.17g, exp10f = %.9g\n", ok ? "ok  " : "FAIL", cases[i].x, d, f);
		if (!ok) failed = 1;
	}
	printf(failed ? "RESULT: FAIL\n" : "RESULT: PASS\n");
	return failed;
}
