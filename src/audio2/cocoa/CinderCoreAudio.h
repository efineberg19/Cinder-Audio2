/*
 Copyright (c) 2013, The Cinder Project

 This code is intended to be used with the Cinder C++ library, http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "audio2/audio.h"

#include <memory>
#include <AudioToolbox/AudioToolbox.h>

struct AudioStreamBasicDescription;

namespace audio2 { namespace cocoa {

class ConverterCoreAudio : public Converter {
public:
	ConverterCoreAudio( const Format &sourceFormat, const Format &destFormat );
	virtual ~ConverterCoreAudio();

	virtual void convert( Buffer *sourceBuffer, Buffer *destBuffer ) override;

private:
	::AudioConverterRef mAudioConverter;
};

//! convience function for pretty printing \a asbd
void printASBD( const ::AudioStreamBasicDescription &asbd );

struct AudioBufferListDeleter {
	void operator()( ::AudioBufferList *bufferList )
	{
		for( size_t i = 0; i < bufferList->mNumberBuffers; i++ )
			free( bufferList->mBuffers[i].mData );
		free( bufferList );
	}
};

struct AudioBufferListShallowDeleter {
	void operator()( ::AudioBufferList *bufferList )
	{
		free( bufferList );
	}
};

typedef std::unique_ptr<::AudioBufferList, AudioBufferListDeleter> AudioBufferListPtr;
typedef std::unique_ptr<::AudioBufferList, AudioBufferListShallowDeleter> AudioBufferListShallowPtr;

AudioBufferListPtr createNonInterleavedBufferList( size_t numChannels, size_t numFrames );
AudioBufferListShallowPtr createNonInterleavedBufferListShallow( size_t numChannels );

::AudioComponent findAudioComponent( const ::AudioComponentDescription &componentDescription );
void findAndCreateAudioComponent( const ::AudioComponentDescription &componentDescription, ::AudioComponentInstance *componentInstance );

::AudioStreamBasicDescription createFloatAsbd( size_t numChannels, size_t sampleRate, bool isInterleaved = false );

inline void copyToBufferList( ::AudioBufferList *bufferList, const Buffer *buffer )
{
	if( buffer->getLayout() == Buffer::Layout::Interleaved ) {
		CI_ASSERT( bufferList->mNumberBuffers == 1 );
		memcpy( bufferList->mBuffers[0].mData, buffer->getData(), bufferList->mBuffers[0].mDataByteSize );
	}
	else {
		for( UInt32 i = 0; i < bufferList->mNumberBuffers; i++ )
			memcpy( bufferList->mBuffers[i].mData, buffer->getChannel( i ), bufferList->mBuffers[i].mDataByteSize );
	}
}

inline void copyFromBufferList( Buffer *buffer, const ::AudioBufferList *bufferList )
{
	if( buffer->getLayout() == Buffer::Layout::Interleaved ) {
		CI_ASSERT( bufferList->mNumberBuffers == 1 );
		memcpy( buffer->getData(), bufferList->mBuffers[0].mData, bufferList->mBuffers[0].mDataByteSize );
	}
	else {
		for( UInt32 i = 0; i < bufferList->mNumberBuffers; i++ )
			memcpy( buffer->getChannel( i ), bufferList->mBuffers[i].mData, bufferList->mBuffers[i].mDataByteSize );
	}
}

} } // namespace audio2::cocoa