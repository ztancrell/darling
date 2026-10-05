#include <CoreServices/CoreServicesPriv.h>

// TODO: Security.framework expects this symbol to be available in
// /System/Library/Frameworks/CoreServices.framework/Frameworks/CarbonCore.framework/Versions/A/CarbonCore

Boolean _CSCheckFix(CFStringRef str)
{
	// Checks whether given issue number is fixed in this release or something
	return 0;
}


Boolean _CSCheckFixDisable(UInt32 fix)
{
	// Whether a compatibility fix has been switched off for this process; none ever is
	return 0;
}
