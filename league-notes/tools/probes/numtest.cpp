// Do Darling's number functions behave? ANGLE's shader lexer relies on these when it reads "0.0", "1.0", "0.5".
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <sstream>
#include <string>
#include <locale>

int main() {
	const char* inputs[] = { "0.0", "1.0", "0.5", "1e-45", "3.4e38", "1e39", "0.000001" };
	for (const char* in : inputs) {
		errno = 0; char* end = nullptr;
		float f = strtof(in, &end); double d = strtod(in, nullptr);
		std::istringstream s(in); s.imbue(std::locale::classic()); double sd = -1; s >> sd;
		char buf[64]; int n = snprintf(buf, sizeof buf, "%g", d);
		printf("%-9s strtof=%g (consumed %zu, errno %d) strtod=%g istream=%g%s snprintf=\"%s\"(%d)\n", in, f, (size_t)(end - in), errno, d, sd, s.fail() ? " FAIL" : "", buf, n);
	}
	for (int e : { 0, 1, -1, 10, -10, 38, -38, -45 })
		printf("pow(10,%d)=%g exp10=%g powf=%g\n", e, pow(10.0, e), __exp10(e), powf(10.0f, (float)e));
	printf("isfinite(inf)=%d isinf=%d isnan(nan)=%d FLT_MAX*10 isinf=%d\n", std::isfinite(INFINITY), std::isinf(INFINITY), std::isnan(NAN), std::isinf(3.4e38f * 10.0f));
	std::ostringstream o; o.imbue(std::locale::classic()); o.precision(9); o << 0.5f << " " << 1e-45f << " " << 3.4e38f; printf("ostream: %s\n", o.str().c_str());
	return 0;
}
