/*
 This file is part of Darling.

 Copyright (C) 2020 Lubos Dolezel

 Darling is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Darling is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Darling.  If not, see <http://www.gnu.org/licenses/>.
*/

#import <IOSurface/IOSurface.h>
#import <IOSurface/IOSurfaceObjC.h>
#import <Foundation/Foundation.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <stdatomic.h>
#include <unistd.h>

// A surface is one Mach memory entry: a header page followed by the pixels. The memory entry's
// port is the surface's Mach port, so any process holding it can map the same memory.
// No service is involved, hence no lookup by ID (IOSurfaceLookup stays unimplemented).

#define SURFACE_MAGIC 0x31465344 // "DSF1"
#define MAX_PLANES 4
#define MAX_ALLOC_SIZE (1ull << 32)

struct Plane
{
	uint64_t offset;
	uint32_t width;
	uint32_t height;
	uint32_t bytesPerRow;
	uint32_t bytesPerElement;
};

struct SurfaceHeader
{
	uint32_t magic;
	IOSurfaceID surfaceID;
	uint32_t pixelFormat;
	// As described by the creator; 0 for a surface created without plane info,
	// in which case planes[0] describes the whole surface.
	uint32_t planeCount;
	uint64_t allocSize;
	struct Plane planes[MAX_PLANES];
	// The only fields read from shared memory after the surface is opened.
	_Atomic int32_t useCount;
	_Atomic uint32_t seed;
};

typedef struct
{
	// Private copy, checked once. The shared header is writable by every process
	// that has the port (a sandboxed renderer, say), so it is never trusted again.
	struct SurfaceHeader info;
	struct SurfaceHeader* shared;
	mach_vm_size_t mappedSize;
	mach_port_t port;
	_Atomic int32_t localUseCount;
	NSMutableDictionary* attachments;
} ImplData;

static BOOL headerIsSane(const struct SurfaceHeader* h)
{
	if (h->magic != SURFACE_MAGIC || h->planeCount > MAX_PLANES)
		return NO;
	if (h->allocSize == 0 || h->allocSize > MAX_ALLOC_SIZE)
		return NO;

	uint32_t count = h->planeCount ? h->planeCount : 1;
	for (uint32_t i = 0; i < count; i++)
	{
		const struct Plane* p = &h->planes[i];
		if (p->width == 0 || p->height == 0 || p->bytesPerElement == 0)
			return NO;
		// All operands are 32-bit, so these cannot overflow 64 bits.
		if ((uint64_t) p->width * p->bytesPerElement > p->bytesPerRow)
			return NO;
		uint64_t bytes = (uint64_t) p->bytesPerRow * p->height;
		if (bytes > h->allocSize || p->offset > h->allocSize - bytes)
			return NO;
	}
	return YES;
}

static uint32_t number(NSDictionary* dict, CFStringRef key)
{
	id value = [dict objectForKey: (id) key];
	return [value isKindOfClass: [NSNumber class]] ? [value unsignedIntValue] : 0;
}

static uint32_t alignedBytesPerRow(uint64_t bytes)
{
	bytes = (bytes + 15) & ~15ull;
	return bytes > UINT32_MAX ? 0 : (uint32_t) bytes;
}

size_t IOSurfaceAlignProperty(CFStringRef property, size_t value)
{
	if (CFEqual(property, kIOSurfaceBytesPerRow) || CFEqual(property, kIOSurfacePlaneBytesPerRow))
		return (value + 15) & ~(size_t)15;
	if (CFEqual(property, kIOSurfaceAllocSize) || CFEqual(property, kIOSurfacePlaneSize))
		return round_page(value);
	return value;
}

IOSurfaceID IOSurfaceGetID(IOSurfaceRef buffer)
{
	return buffer ? ((ImplData*) ((IOSurface*) buffer)->_impl)->info.surfaceID : 0;
}

size_t IOSurfaceGetAllocSize(IOSurfaceRef buffer)
{
	return buffer ? ((ImplData*) ((IOSurface*) buffer)->_impl)->info.allocSize : 0;
}

// The caller owns the returned send right.
mach_port_t IOSurfaceCreateMachPort(IOSurfaceRef buffer)
{
	if (!buffer)
		return MACH_PORT_NULL;
	ImplData* data = (ImplData*) ((IOSurface*) buffer)->_impl;
	if (mach_port_mod_refs(mach_task_self(), data->port, MACH_PORT_RIGHT_SEND, 1) != KERN_SUCCESS)
		return MACH_PORT_NULL;
	return data->port;
}

@implementation IOSurface

// Takes over one send right for `port`. On failure releases it and self.
- (nullable instancetype)initWithInfo:(const struct SurfaceHeader*)info port:(mach_port_t)port create:(BOOL)create
{
	mach_vm_address_t address = 0;
	mach_vm_size_t size = vm_page_size + round_page(info->allocSize);

	// Refused when the entry is smaller than the header claims.
	kern_return_t ret = mach_vm_map(mach_task_self(), &address, size, 0, VM_FLAGS_ANYWHERE, port, 0, FALSE,
		VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
	if (ret != KERN_SUCCESS)
	{
		mach_port_deallocate(mach_task_self(), port);
		[self release];
		return nil;
	}

	ImplData* data = (ImplData*) calloc(1, sizeof(ImplData));
	data->info = *info;
	data->shared = (struct SurfaceHeader*) address;
	data->mappedSize = size;
	data->port = port;
	if (create)
		*data->shared = *info;
	self->_impl = data;
	return self;
}

- (nullable instancetype)initWithProperties:(NSDictionary <IOSurfacePropertyKey, id> *)properties
{
	// ponytail: IDs are unique per process pair only by pid; nothing looks surfaces up by ID.
	// A real global ID needs a service (iokitd's IOSurfaceRoot).
	static _Atomic uint32_t counter;
	struct SurfaceHeader h = {
		.magic = SURFACE_MAGIC,
		.surfaceID = ((uint32_t) getpid() << 16) | (++counter & 0xffff),
		.pixelFormat = number(properties, kIOSurfacePixelFormat),
	};
	uint64_t end = 0;

	NSArray* planeInfo = [properties objectForKey: (id) kIOSurfacePlaneInfo];
	if (![planeInfo isKindOfClass: [NSArray class]])
		planeInfo = nil;
	if ([planeInfo count] > MAX_PLANES)
	{
		[self release];
		return nil;
	}
	h.planeCount = [planeInfo count];

	if (h.planeCount == 0)
	{
		struct Plane* p = &h.planes[0];
		p->width = number(properties, kIOSurfaceWidth);
		p->height = number(properties, kIOSurfaceHeight);
		p->bytesPerElement = number(properties, kIOSurfaceBytesPerElement);
		if (p->bytesPerElement == 0 && h.pixelFormat == 'BGRA')
			p->bytesPerElement = 4;
		p->bytesPerRow = number(properties, kIOSurfaceBytesPerRow);
		if (p->bytesPerRow == 0)
			p->bytesPerRow = alignedBytesPerRow((uint64_t) p->width * p->bytesPerElement);
		end = (uint64_t) p->bytesPerRow * p->height;
	}
	for (uint32_t i = 0; i < h.planeCount; i++)
	{
		NSDictionary* plane = [planeInfo objectAtIndex: i];
		if (![plane isKindOfClass: [NSDictionary class]])
		{
			[self release];
			return nil;
		}
		struct Plane* p = &h.planes[i];
		p->width = number(plane, kIOSurfacePlaneWidth);
		p->height = number(plane, kIOSurfacePlaneHeight);
		p->bytesPerElement = number(plane, kIOSurfacePlaneBytesPerElement);
		p->bytesPerRow = number(plane, kIOSurfacePlaneBytesPerRow);
		if (p->bytesPerRow == 0)
			p->bytesPerRow = alignedBytesPerRow((uint64_t) p->width * p->bytesPerElement);
		// Without an explicit offset, planes follow one another.
		p->offset = [plane objectForKey: (id) kIOSurfacePlaneOffset] ? number(plane, kIOSurfacePlaneOffset) : end;
		uint64_t planeEnd = p->offset + (uint64_t) p->bytesPerRow * p->height;
		if (planeEnd > end)
			end = planeEnd;
	}

	h.allocSize = number(properties, kIOSurfaceAllocSize);
	if (h.allocSize < end)
		h.allocSize = end;

	if (!headerIsSane(&h))
	{
		[self release];
		return nil;
	}

	mach_port_t port = MACH_PORT_NULL;
	memory_object_size_t size = vm_page_size + round_page(h.allocSize);
	kern_return_t ret = mach_make_memory_entry_64(mach_task_self(), &size, 0,
		MAP_MEM_NAMED_CREATE | VM_PROT_READ | VM_PROT_WRITE, &port, MACH_PORT_NULL);
	if (ret != KERN_SUCCESS)
	{
		[self release];
		return nil;
	}

	return [self initWithInfo: &h port: port create: YES];
}

// Does not consume the caller's right.
- (nullable instancetype)initWithMachPort:(mach_port_t)port
{
	mach_vm_address_t address = 0;
	struct SurfaceHeader h;

	// Read the header on its own first: it says how much to map.
	kern_return_t ret = mach_vm_map(mach_task_self(), &address, vm_page_size, 0, VM_FLAGS_ANYWHERE, port, 0, FALSE,
		VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
	if (ret != KERN_SUCCESS)
	{
		[self release];
		return nil;
	}
	memcpy(&h, (void*) address, sizeof(h));
	mach_vm_deallocate(mach_task_self(), address, vm_page_size);

	if (!headerIsSane(&h) || mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, 1) != KERN_SUCCESS)
	{
		[self release];
		return nil;
	}

	return [self initWithInfo: &h port: port create: NO];
}

// No service to ask; see the comment at the top.
- (nullable instancetype)initWithSurfaceID:(IOSurfaceID)surfaceID
{
	[self release];
	return nil;
}

- (void) dealloc
{
	ImplData* data = (ImplData*) _impl;
	if (data != NULL)
	{
		// ponytail: a process that dies holding a use count leaves the surface "in use" for good.
		// Fixing that needs an owner that outlives clients (a service).
		if (data->localUseCount > 0)
			atomic_fetch_sub(&data->shared->useCount, data->localUseCount);
		mach_vm_deallocate(mach_task_self(), (mach_vm_address_t) data->shared, data->mappedSize);
		mach_port_deallocate(mach_task_self(), data->port);
		[data->attachments release];
		free(data);
	}
	[super dealloc];
}

- (NSUInteger) hash
{
	ImplData* data = (ImplData*) _impl;
	return data->info.surfaceID;
}

- (BOOL)isEqual:(id)object
{
	if (![object isKindOfClass: [self class]])
		return NO;

	IOSurface* that = (IOSurface*) object;
	return ((ImplData*) self->_impl)->info.surfaceID == ((ImplData*) that->_impl)->info.surfaceID;
}

- (CFTypeID) _cfTypeID
{
    return IOSurfaceGetTypeID();
}

- (NSInteger) width
{
	return [self widthOfPlaneAtIndex: 0];
}

- (NSInteger) height
{
	return [self heightOfPlaneAtIndex: 0];
}

- (void *) baseAddress
{
	return [self baseAddressOfPlaneAtIndex: 0];
}

- (NSInteger) bytesPerRow
{
	return [self bytesPerRowOfPlaneAtIndex: 0];
}

- (NSInteger) bytesPerElement
{
	return [self bytesPerElementOfPlaneAtIndex: 0];
}

- (NSInteger) elementWidth
{
	return [self elementWidthOfPlaneAtIndex: 0];
}

- (NSInteger) elementHeight
{
	return [self elementHeightOfPlaneAtIndex: 0];
}

- (OSType) pixelFormat
{
	return ((ImplData*) _impl)->info.pixelFormat;
}

- (NSUInteger) planeCount
{
	return ((ImplData*) _impl)->info.planeCount;
}

- (NSInteger) allocationSize
{
	return ((ImplData*) _impl)->info.allocSize;
}

// Plane 0 of a surface without planes is the surface itself.
- (const struct Plane*) planeAtIndex:(NSUInteger)planeIndex
{
	ImplData* data = (ImplData*) _impl;
	if (planeIndex >= (data->info.planeCount ? data->info.planeCount : 1))
		return NULL;
	return &data->info.planes[planeIndex];
}

- (NSInteger)widthOfPlaneAtIndex:(NSUInteger)planeIndex
{
	const struct Plane* p = [self planeAtIndex: planeIndex];
	return p ? p->width : 0;
}

- (NSInteger)heightOfPlaneAtIndex:(NSUInteger)planeIndex
{
	const struct Plane* p = [self planeAtIndex: planeIndex];
	return p ? p->height : 0;
}

- (NSInteger)bytesPerRowOfPlaneAtIndex:(NSUInteger)planeIndex
{
	const struct Plane* p = [self planeAtIndex: planeIndex];
	return p ? p->bytesPerRow : 0;
}

- (NSInteger)bytesPerElementOfPlaneAtIndex:(NSUInteger)planeIndex
{
	const struct Plane* p = [self planeAtIndex: planeIndex];
	return p ? p->bytesPerElement : 0;
}

- (NSInteger)elementWidthOfPlaneAtIndex:(NSUInteger)planeIndex
{
	return 1;
}

- (NSInteger)elementHeightOfPlaneAtIndex:(NSUInteger)planeIndex
{
	return 1;
}

- (void *)baseAddressOfPlaneAtIndex:(NSUInteger)planeIndex
{
	const struct Plane* p = [self planeAtIndex: planeIndex];
	if (!p)
		return NULL;
	return (uint8_t*) ((ImplData*) _impl)->shared + vm_page_size + p->offset;
}

- (void)incrementUseCount
{
	ImplData* data = (ImplData*) _impl;
	data->localUseCount++;
	atomic_fetch_add(&data->shared->useCount, 1);
}

- (void)decrementUseCount
{
	ImplData* data = (ImplData*) _impl;
	data->localUseCount--;
	atomic_fetch_sub(&data->shared->useCount, 1);
}

- (int32_t) localUseCount
{
	ImplData* data = (ImplData*) _impl;
	return data->localUseCount;
}

- (BOOL) isInUse
{
	return ((ImplData*) _impl)->shared->useCount > 0;
}

- (uint32_t) seed
{
	return ((ImplData*) _impl)->shared->seed;
}

// The pixels are ordinary shared memory, always mapped: there is nothing to page in or
// synchronise with a GPU copy, so locking only maintains the change counter.
- (kern_return_t)lockWithOptions:(IOSurfaceLockOptions)options seed:(nullable uint32_t *)seed
{
	if (seed)
		*seed = [self seed];
	return kIOSurfaceSuccess;
}

- (kern_return_t)unlockWithOptions:(IOSurfaceLockOptions)options seed:(nullable uint32_t *)seed
{
	ImplData* data = (ImplData*) _impl;
	if (!(options & kIOSurfaceLockReadOnly))
		atomic_fetch_add(&data->shared->seed, 1);
	if (seed)
		*seed = [self seed];
	return kIOSurfaceSuccess;
}

// ponytail: attachments stay in this process. Sharing them (colour space set by the producer,
// read by the consumer) needs room in the shared region or a service.
- (NSMutableDictionary*) _attachments
{
	ImplData* data = (ImplData*) _impl;
	if (!data->attachments)
		data->attachments = [NSMutableDictionary new];
	return data->attachments;
}

- (void)setAttachment:(id)anObject forKey:(NSString *)key
{
	@synchronized(self)
	{
		[[self _attachments] setObject: anObject forKey: key];
	}
}

- (nullable id)attachmentForKey:(NSString *)key
{
	@synchronized(self)
	{
		return [[[[self _attachments] objectForKey: key] retain] autorelease];
	}
}

- (void)removeAttachmentForKey:(NSString *)key
{
	@synchronized(self)
	{
		[[self _attachments] removeObjectForKey: key];
	}
}

- (void)setAllAttachments:(NSDictionary<NSString *, id> *)dict
{
	@synchronized(self)
	{
		[[self _attachments] setDictionary: dict];
	}
}

- (nullable NSDictionary<NSString *, id> *)allAttachments
{
	@synchronized(self)
	{
		return [[[self _attachments] copy] autorelease];
	}
}

- (void)removeAllAttachments
{
	@synchronized(self)
	{
		[[self _attachments] removeAllObjects];
	}
}

- (BOOL) allowsPixelSizeCasting
{
	return NO;
}

// The memory is never discarded.
- (kern_return_t)setPurgeable:(IOSurfacePurgeabilityState)newState oldState:(IOSurfacePurgeabilityState * _Nullable)oldState
{
	if (oldState)
		*oldState = kIOSurfacePurgeableNonVolatile;
	return kIOSurfaceSuccess;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector {
    return [NSMethodSignature signatureWithObjCTypes: "v@:"];
}

- (void)forwardInvocation:(NSInvocation *)anInvocation {
    NSLog(@"Stub called: %@ in %@", NSStringFromSelector([anInvocation selector]), [self class]);
}

@end
