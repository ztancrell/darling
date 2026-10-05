#include <AE/AE.h>

static int verbose = 0;
__attribute__((constructor)) static void initme(void) {
    verbose = getenv("STUB_VERBOSE") != NULL;
}

OSErr AEInstallEventHandler(
  AEEventClass theAEEventClass,
  AEEventID theAEEventID,
  AEEventHandlerUPP handler,
  SRefCon handlerRefcon,
  Boolean isSysHandler
) {
    if (verbose) puts("STUB: AEInstallEventHandler called");
    return noErr;
}

/* ---- Darling: local discovery placeholders, NOT implementations ----
 * Each logs its own name when called and returns zero, so one run shows which of these an
 * application actually reaches. Replace each with a real implementation, or remove it. */
#include <stdio.h>
#ifndef DARLING_UNIMPLEMENTED
#define DARLING_UNIMPLEMENTED(sym) \
	__attribute__((visibility("default"))) long darling_unimplemented_##sym(void) __asm__("_" #sym); \
	long darling_unimplemented_##sym(void) { fprintf(stderr, "DARLING unimplemented function called: " #sym "\n"); return 0; }
#endif
DARLING_UNIMPLEMENTED(AERemoveEventHandler)
