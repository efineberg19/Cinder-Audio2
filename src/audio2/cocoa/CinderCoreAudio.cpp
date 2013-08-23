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

#include "audio2/cocoa/CinderCoreAudio.h"
#include "audio2/Debug.h"

using namespace std;
using namespace ci;

namespace cinder { namespace audio2 { namespace cocoa {

// ----------------------------------------------------------------------------------------------------
// MARK: - ConverterCoreAudio
// ----------------------------------------------------------------------------------------------------

ConverterCoreAudio::ConverterCoreAudio( const Format &sourceFormat, const Format &destFormat )
: Converter( sourceFormat, destFormat ), mAudioConverter( nullptr )
{
	::AudioStreamBasicDescription sourceAsbd = createFloatAsbd( sourceFormat.getChannels(), sourceFormat.getSampleRate() );
	::AudioStreamBasicDescription destAsbd = createFloatAsbd( destFormat.getChannels(), destFormat.getSampleRate() );

	OSStatus status = ::AudioConverterNew( &sourceAsbd, &destAsbd, &mAudioConverter );
	CI_ASSERT( status == noErr );


	UInt32 maxPacketSize;
	UInt32 size = sizeof( maxPacketSize );
	status = ::AudioConverterGetProperty( mAudioConverter, kAudioConverterPropertyMaximumOutputPacketSize, &size, &maxPacketSize );
	CI_ASSERT( status == noErr );

	LOG_V << "max packet size: " << maxPacketSize << endl;
}

ConverterCoreAudio::~ConverterCoreAudio()
{
	if( mAudioConverter ) {
		OSStatus status = AudioConverterDispose( mAudioConverter );
		CI_ASSERT( status == noErr );
	}
}

void ConverterCoreAudio::convert( Buffer *sourceBuffer, Buffer *destBuffer )
{
	if( mSourceFormat.getSampleRate() == mDestFormat.getSampleRate() ) {
		UInt32 inputDataSize = sourceBuffer->getSize() * sizeof( Buffer::SampleType );
		UInt32 outputDataSize = destBuffer->getSize() * sizeof( Buffer::SampleType );
		OSStatus status = ::AudioConverterConvertBuffer( mAudioConverter, inputDataSize, sourceBuffer->getData(), &outputDataSize, destBuffer->getData() );
		CI_ASSERT( status == noErr );
	} else {
		CI_ASSERT( 0 && "not implemented" ); // TODO: samplerate / VBR conversion
//		OSStatus status = ::AudioConverterFillComplexBuffer( mAudioConverter, converterCallback, &converterInfo, &ioOutputDataPackets, &bufferList, converterInfo.inputFilePacketDescriptions );
//		CI_ASSERT( status == noErr );
	}
}

// ----------------------------------------------------------------------------------------------------
// MARK: - Utility functions
// ----------------------------------------------------------------------------------------------------


void printASBD( const ::AudioStreamBasicDescription &asbd ) {
	char formatIDString[5];
	UInt32 formatID = CFSwapInt32HostToBig (asbd.mFormatID);
	bcopy (&formatID, formatIDString, 4);
	formatIDString[4] = '\0';

	printf( "  Sample Rate:         %10.0f\n",  asbd.mSampleRate );
	printf( "  Format ID:           %10s\n",    formatIDString );
	printf( "  Format Flags:        %10X\n",    (unsigned int)asbd.mFormatFlags );
	printf( "  Bytes per Packet:    %10d\n",    (unsigned int)asbd.mBytesPerPacket );
	printf( "  Frames per Packet:   %10d\n",    (unsigned int)asbd.mFramesPerPacket );
	printf( "  Bytes per Frame:     %10d\n",    (unsigned int)asbd.mBytesPerFrame );
	printf( "  Channels per Frame:  %10d\n",    (unsigned int)asbd.mChannelsPerFrame );
	printf( "  Bits per Channel:    %10d\n",    (unsigned int)asbd.mBitsPerChannel );
}

AudioBufferListPtr createNonInterleavedBufferList( size_t numChannels, size_t numFrames )
{
	::AudioBufferList *bufferList = static_cast<::AudioBufferList *>( calloc( 1, sizeof( ::AudioBufferList ) + sizeof( ::AudioBuffer ) * (numChannels - 1) ) );
	bufferList->mNumberBuffers = static_cast<UInt32>( numChannels );
	for( size_t i = 0; i < numChannels; i++ ) {
		::AudioBuffer *buffer = &bufferList->mBuffers[i];
		buffer->mNumberChannels = 1;
		buffer->mDataByteSize = static_cast<UInt32>( numFrames * sizeof( float ) );
		buffer->mData = malloc( numFrames * sizeof( float ) );
	}

	return AudioBufferListPtr( bufferList );
}

AudioBufferListShallowPtr createNonInterleavedBufferListShallow( size_t numChannels )
{
	::AudioBufferList *bufferList = static_cast<::AudioBufferList *>( calloc( 1, sizeof( ::AudioBufferList ) + sizeof( ::AudioBuffer ) * (numChannels - 1) ) );
	bufferList->mNumberBuffers = static_cast<UInt32>( numChannels );
	for( size_t i = 0; i < numChannels; i++ ) {
		::AudioBuffer *buffer = &bufferList->mBuffers[i];
		buffer->mNumberChannels = 1;
		buffer->mDataByteSize = 0;
		buffer->mData = nullptr;
	}

	return AudioBufferListShallowPtr( bufferList );
}

::AudioComponent findAudioComponent( const ::AudioComponentDescription &componentDescription )
{
	::AudioComponent component = ::AudioComponentFindNext( NULL, &componentDescription );
	CI_ASSERT( component );
	return component;
}

void findAndCreateAudioComponent( const ::AudioComponentDescription &componentDescription, ::AudioComponentInstance *componentInstance )
{
	::AudioComponent component = findAudioComponent( componentDescription );
	OSStatus status = ::AudioComponentInstanceNew( component, componentInstance );
	CI_ASSERT( status == noErr );
}

::AudioStreamBasicDescription createFloatAsbd( size_t numChannels, size_t sampleRate, bool isInterleaved )
{
	const size_t kBytesPerSample = sizeof( float );
	::AudioStreamBasicDescription asbd{ 0 };
	asbd.mSampleRate = sampleRate;
	asbd.mFormatID = kAudioFormatLinearPCM;
	asbd.mFramesPerPacket    = 1;
	asbd.mChannelsPerFrame = numChannels;
	asbd.mBitsPerChannel     = 8 * kBytesPerSample;

	if( isInterleaved ) {
		asbd.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kLinearPCMFormatFlagIsPacked;
		asbd.mBytesPerPacket = kBytesPerSample * numChannels;
		asbd.mBytesPerFrame = kBytesPerSample * numChannels;
	}
	else {
		// paraphrasing comment in CoreAudioTypes.h: for non-interleaved, the ABSD describes the format of
		// one AudioBuffer that is contained with the AudioBufferList, each AudioBuffer is a mono signal.
		asbd.mFormatFlags        = kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
		asbd.mBytesPerPacket     = kBytesPerSample;
		asbd.mBytesPerFrame      = kBytesPerSample;
	}

	return asbd;
}

} } } // namespace cinder::audio2::cocoa