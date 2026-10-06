#include <DiskArbitration/DiskArbitration.h>
#include <CoreFoundation/CoreFoundation.h>

// Darling has no disk arbitration daemon: no disk ever appears, changes or disappears, so the
// registrations below never fire, and requests on a disk are refused as unsupported.

CFDictionaryRef kDADiskDescriptionMatchVolumeMountable;
CFArrayRef kDADiskDescriptionWatchVolumePath;

__attribute__((constructor))
static void DACallbacksInitialize(void)
{
	const void *key = kDADiskDescriptionVolumeMountableKey;
	const void *yes = kCFBooleanTrue;
	const void *path = kDADiskDescriptionVolumePathKey;

	kDADiskDescriptionMatchVolumeMountable = CFDictionaryCreate(NULL, &key, &yes, 1,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	kDADiskDescriptionWatchVolumePath = CFArrayCreate(NULL, &path, 1, &kCFTypeArrayCallBacks);
}

void DARegisterDiskAppearedCallback(DASessionRef session, CFDictionaryRef match,
	DADiskAppearedCallback callback, void *context)
{
}

void DARegisterDiskDescriptionChangedCallback(DASessionRef session, CFDictionaryRef match,
	CFArrayRef watch, DADiskDescriptionChangedCallback callback, void *context)
{
}

void DARegisterDiskDisappearedCallback(DASessionRef session, CFDictionaryRef match,
	DADiskDisappearedCallback callback, void *context)
{
}

static void refuse(DADiskRef disk, void (*callback)(DADiskRef, DADissenterRef, void *), void *context)
{
	if (!callback)
		return;
	DADissenterRef dissenter = DADissenterCreate(NULL, kDAReturnUnsupported, NULL);
	callback(disk, dissenter, context);
	if (dissenter)
		CFRelease(dissenter);
}

void DADiskUnmount(DADiskRef disk, DADiskUnmountOptions options, DADiskUnmountCallback callback, void *context)
{
	refuse(disk, callback, context);
}

void DADiskEject(DADiskRef disk, DADiskEjectOptions options, DADiskEjectCallback callback, void *context)
{
	refuse(disk, callback, context);
}

void DADiskClaim(DADiskRef disk, DADiskClaimOptions options, DADiskClaimReleaseCallback release,
	void *releaseContext, DADiskClaimCallback callback, void *callbackContext)
{
	refuse(disk, callback, callbackContext);
}

void DADiskUnclaim(DADiskRef disk)
{
}
