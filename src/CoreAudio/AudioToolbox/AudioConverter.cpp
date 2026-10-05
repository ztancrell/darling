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

#include "AudioConverter.h"
#include "AudioConverterImpl.h"
#include <CarbonCore/MacErrors.h>
#include <stdexcept>
#include <cstring>
#include <mutex>
#include <unordered_set>

// A converter reference is checked against the converters that exist before it is used, as on macOS,
// where references are looked up in a table and an unknown one gets an error back. Chromium 108 relies
// on that: its AudioToolbox decoder disposes a never-initialised reference in its destructor.
static std::mutex g_convertersLock;
static std::unordered_set<AudioConverter*> g_converters;

static AudioConverter* resolve(AudioConverterRef ref)
{
	std::lock_guard<std::mutex> guard(g_convertersLock);
	AudioConverter* converter = (AudioConverter*)ref;
	return g_converters.count(converter) ? converter : nullptr;
}

#define RESOLVE_OR_FAIL(ref) \
	AudioConverter* converter = resolve(ref); \
	if (!converter) \
		return kAudio_ParamError

OSStatus AudioConverterNew(const AudioStreamBasicDescription* inSourceFormat, const AudioStreamBasicDescription* inDestinationFormat, AudioConverterRef* outAudioConverter)
{
	OSStatus status = AudioConverter::create(inSourceFormat, inDestinationFormat, (AudioConverter**)outAudioConverter);
	if (status == noErr && *outAudioConverter)
	{
		std::lock_guard<std::mutex> guard(g_convertersLock);
		g_converters.insert((AudioConverter*)*outAudioConverter);
	}
	return status;
}

OSStatus AudioConverterNewSpecific(const AudioStreamBasicDescription* inSourceFormat, const AudioStreamBasicDescription* inDestinationFormat, UInt32 inNumberClassDescriptions, const AudioClassDescription* nClassDescriptions, AudioConverterRef* outAudioConverter)
{
	// FIXME
	return AudioConverterNew(inSourceFormat, inDestinationFormat, outAudioConverter);
}

OSStatus AudioConverterReset(AudioConverterRef inAudioConverter)
{
	RESOLVE_OR_FAIL(inAudioConverter);
	if (inAudioConverter)
	{
		converter->flush();
		return noErr;
	}
	else
		return paramErr;
}

OSStatus AudioConverterDispose(AudioConverterRef inAudioConverter)
{
	AudioConverter* converter = (AudioConverter*)inAudioConverter;
	{
		std::lock_guard<std::mutex> guard(g_convertersLock);
		if (!g_converters.erase(converter))
			return kAudio_ParamError;
	}
	delete converter;
	return noErr;
}

OSStatus AudioConverterGetProperty(AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID, UInt32 *ioPropertyDataSize, void *outPropertyData)
{
	RESOLVE_OR_FAIL(inAudioConverter);
	return converter->getProperty(inPropertyID, ioPropertyDataSize, outPropertyData);
}

OSStatus AudioConverterGetPropertyInfo(AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID, UInt32 *outSize, Boolean *outWritable)
{
	RESOLVE_OR_FAIL(inAudioConverter);
	return converter->getPropertyInfo(inPropertyID, outSize, outWritable);
}

OSStatus AudioConverterSetProperty(AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID, UInt32 inPropertyDataSize, const void *inPropertyData)
{
	RESOLVE_OR_FAIL(inAudioConverter);
	return converter->setProperty(inPropertyID, inPropertyDataSize, inPropertyData);
}

OSStatus AudioConverterConvertBuffer(AudioConverterRef inAudioConverter, UInt32 inInputDataSize, const void* inInputData, UInt32 *ioOutputDataSize, void *outOutputData)
{
	RESOLVE_OR_FAIL(inAudioConverter);
	AudioBufferList in, out;
	OSStatus stat;

	in.mNumberBuffers = out.mNumberBuffers = 1;

	in.mBuffers[0].mNumberChannels = 2;
	out.mBuffers[0].mNumberChannels = 2;

	in.mBuffers[0].mDataByteSize = inInputDataSize;
	in.mBuffers[0].mData = const_cast<void*>(inInputData);

	out.mBuffers[0].mDataByteSize = *ioOutputDataSize;
	out.mBuffers[0].mData = outOutputData;

	stat = AudioConverterConvertComplexBuffer(inAudioConverter, inInputDataSize/converter->frameSize(), &in, &out);
	*ioOutputDataSize = out.mBuffers[0].mDataByteSize;

	return stat;
}

OSStatus AudioConverterFillComplexBuffer(AudioConverterRef inAudioConverter, AudioConverterComplexInputDataProc inInputDataProc, void *inInputDataProcUserData, UInt32 *ioOutputDataPacketSize, AudioBufferList *outOutputData, AudioStreamPacketDescription* outPacketDescription)
{
	RESOLVE_OR_FAIL(inAudioConverter);
	return converter->fillComplex(inInputDataProc, inInputDataProcUserData, ioOutputDataPacketSize, outOutputData, outPacketDescription);
}

OSStatus AudioConverterConvertComplexBuffer(AudioConverterRef inAudioConverter, UInt32 inNumberPCMFrames, const AudioBufferList *inInputData, AudioBufferList *outOutputData)
{
	RESOLVE_OR_FAIL(inAudioConverter);
	OSStatus status;
	UInt32 dataPacketSize;
	int64_t totalOutBytes = 0;
	typedef OSStatus (^CallbackBlock)(AudioConverterRef audioConverter, UInt32* numberDataPackets, AudioBufferList* data, AudioStreamPacketDescription** dataPacketDescription);
	
	CallbackBlock callbackBlock =
		^ OSStatus (AudioConverterRef audioConverter, UInt32* numberDataPackets, AudioBufferList* data, AudioStreamPacketDescription** dataPacketDescription)
	{
		const AudioBufferList *inInputData = (AudioBufferList*) inInputData;

		if (dataPacketDescription)
			*dataPacketDescription = nullptr;
		
		// TODO

		return unimpErr;
	};

	// Squeezing a block through a function pointer
	AudioConverterComplexInputDataProc proc = [](AudioConverterRef audioConverter, UInt32* numberDataPackets, AudioBufferList* data, AudioStreamPacketDescription** dataPacketDescription, void* userData) -> OSStatus
	{
		CallbackBlock callbackBlock = (CallbackBlock) userData;
		return callbackBlock(audioConverter, numberDataPackets, data, dataPacketDescription);
	};

	for (UInt32 i = 0; i < outOutputData->mNumberBuffers; i++)
		totalOutBytes += outOutputData->mBuffers[i].mDataByteSize;

	dataPacketSize = totalOutBytes / converter->frameSize();

	status = AudioConverterFillComplexBuffer(inAudioConverter, proc, (void*) inInputData, &dataPacketSize, outOutputData, nullptr);
	return status;
}
