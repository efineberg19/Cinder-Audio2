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

#include "audio2/msw/FileMediaFoundation.h"
#include "audio2/audio.h"
#include "audio2/Debug.h"

#include <mfidl.h>
#include <mfapi.h>
#include <mfreadwrite.h>
#include <propvarutil.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")

// TODO NEXT: need to closely look at IMFSourceReader in able to decide 
// how to pack audio2::Buffer's via it's ReadSample methods
// - want to have minimal copies when loading a buffer for BufferPlayerNode, so
//   might use a private read method that takes float * and count

using namespace std;
using namespace ci;

namespace cinder { namespace audio2 { namespace msw {

namespace {

inline float nanoSecondsToSeconds( LONGLONG ns )
{
	return (float)ns / 10000000.0f;
} 

inline LONGLONG secondsToNanoSeconds( float seconds )
{
	return (LONGLONG)seconds * 10000000;
}

//inline size_t nanonSecondsToFrames( LONGLONG ns, size_t numChannels )
//{
//
//}

} // anonymous namespace

// ----------------------------------------------------------------------------------------------------
// MARK: - SourceFileMediaFoundation
// ----------------------------------------------------------------------------------------------------

SourceFileMediaFoundation::SourceFileMediaFoundation( const DataSourceRef &dataSource, size_t numChannels, size_t sampleRate )
: SourceFile( dataSource, numChannels, sampleRate ), mReadPos( 0 ), mCanSeek( false ), mSeconds( 0.0f )
{
	initReader( dataSource );

	if( ! mNumChannels )
		mNumChannels = mFileNumChannels;
	if( ! mSampleRate )
		mSampleRate = mFileSampleRate;

	// TODO: need a converter here if there's a channel or samplerate mismatch

	CI_ASSERT( mFileNumChannels == mNumChannels );
	CI_ASSERT( mSampleRate == mFileSampleRate );

	// TODO: use nanoseconds instead of mSeconds
	mNumFrames = static_cast<size_t>( mSeconds * mSampleRate  / mFileNumChannels );

	LOG_V << "complete. total seconds: " << mSeconds << ", frames: " << mNumFrames << ", can seek: " << mCanSeek << endl;
}

SourceFileMediaFoundation::~SourceFileMediaFoundation()
{
	HRESULT hr = ::MFShutdown();
	CI_ASSERT( hr == S_OK );
}

inline bool readWasSuccessful( HRESULT hr, DWORD streamFlags )
{

}

size_t SourceFileMediaFoundation::read( Buffer *buffer )
{
	CI_ASSERT( 0 && "not implemented" );
	return 0;
}

BufferRef SourceFileMediaFoundation::loadBuffer()
{
	//if( mReadPos != 0 )
	//	seek( 0 );

	CI_ASSERT( mNumChannels == 1 && "TODO: multi-channel" );
	
	BufferRef result( new Buffer( mNumFrames, mNumChannels ) );

	float *resultData = result->getData();

	size_t numFramesRead = 0;
	while( numFramesRead != mNumFrames ) {
		size_t readCount = processNextReadSample();
		if( ! readCount )
			break;

		CI_ASSERT( numFramesRead + readCount <= mNumFrames );

		memcpy( resultData + numFramesRead, mReadBuffer.data(), readCount * sizeof( float ) );
		numFramesRead += readCount;
	}

	return result;
}

void SourceFileMediaFoundation::seek( size_t readPosition )
{
	//if( readPosition >= mNumFrames )
	//	return;

	//mReadPos = readPosition;

	if( ! mCanSeek ) {
		LOG_E << "cannot seek." << endl;
		return;
	}

	float positionSeconds = (float)readPosition / (float)mSampleRate;
	if( positionSeconds > mSeconds ) {
		LOG_E << "cannot seek beyond end of file (" << positionSeconds << "s)." << endl;
		return;
	}
	//LOG_V << "seeking to: " << milliseconds << endl;

	LONGLONG position = secondsToNanoSeconds( positionSeconds );
	PROPVARIANT seekVar;
	HRESULT hr = ::InitPropVariantFromInt64( position, &seekVar );
	CI_ASSERT( hr == S_OK );
	hr = mSourceReader->SetCurrentPosition( GUID_NULL, seekVar );
	CI_ASSERT( hr == S_OK );
	hr = PropVariantClear( &seekVar );
	CI_ASSERT( hr == S_OK );
}

void SourceFileMediaFoundation::setSampleRate( size_t sampleRate )
{
	mSampleRate = sampleRate;
	//updateOutputFormat();
}

void SourceFileMediaFoundation::setNumChannels( size_t numChannels )
{
	mNumChannels = numChannels;
	//updateOutputFormat();
}

// TODO: consider moving MFStartup / MFShutdown to a singleton
// - reference count the current number of SourceFileMediaFoundation objects
// - call shutdown at zero

// TODO: test setting MF_LOW_LATENCY attribute
void SourceFileMediaFoundation::initReader( const DataSourceRef &dataSource )
{
	HRESULT hr = ::MFStartup( MF_VERSION ); // TODO: try passing in MFSTARTUP_LITE (no sockets) and see if load is faster
	CI_ASSERT( hr == S_OK );

	::IMFAttributes *attributes;
	hr = ::MFCreateAttributes( &attributes, 1 );
	CI_ASSERT( hr == S_OK );
	auto attributesPtr = makeComUnique( attributes );

	::IMFSourceReader *sourceReader;

	if( dataSource->isFilePath() ) {
		hr = ::MFCreateSourceReaderFromURL( dataSource->getFilePath().wstring().c_str(), attributesPtr.get(), &sourceReader );
		CI_ASSERT( hr == S_OK );
	}
	else {
		CI_ASSERT( 0 && "TODO: MSW resources." );
	}

	mSourceReader = makeComUnique( sourceReader );

	// get files native format
	::IMFMediaType *nativeType;
	::WAVEFORMATEX *fileFormat;
	UINT32 formatSize;
	hr = mSourceReader->GetNativeMediaType( MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &nativeType );
	CI_ASSERT( hr == S_OK );
	hr = ::MFCreateWaveFormatExFromMFMediaType( nativeType, &fileFormat, &formatSize );
	CI_ASSERT( hr == S_OK );

	//mNativeFormat = *nativeFormat;

	GUID outputSubType = MFAudioFormat_PCM; // default to PCM, upgrade if we can
	mSampleFormat = Format::INT_16;
	LOG_V << "native bits per sample: " << fileFormat->wBitsPerSample << endl;
	if( fileFormat->wBitsPerSample == 32 ) {
		mSampleFormat = Format::FLOAT_32;
		outputSubType = MFAudioFormat_Float;
	}

	mFileNumChannels = fileFormat->nChannels;
	mFileSampleRate = fileFormat->nSamplesPerSec;

	LOG_V << "file channels: " << mFileNumChannels << ", samplerate: " << mFileSampleRate << endl;

	::CoTaskMemFree( fileFormat );

	// set output type, which loads the proper decoder:
	::IMFMediaType *outputType;
	hr = ::MFCreateMediaType( &outputType );
	auto outputTypeRef = makeComUnique( outputType );
	hr = outputTypeRef->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
	CI_ASSERT( hr == S_OK );
	hr = outputTypeRef->SetGUID( MF_MT_SUBTYPE, outputSubType );
	CI_ASSERT( hr == S_OK );

	hr = mSourceReader->SetCurrentMediaType( MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, outputTypeRef.get() );
	CI_ASSERT( hr == S_OK );

	// after the decoder is loaded, we have to now get the 'complete' output type before retrieving its format
	// TODO: still needed?
	::IMFMediaType *completeOutputType;
	hr = mSourceReader->GetCurrentMediaType( MF_SOURCE_READER_FIRST_AUDIO_STREAM, &completeOutputType );
	CI_ASSERT( hr == S_OK );

	::WAVEFORMATEX *format;
	hr = MFCreateWaveFormatExFromMFMediaType( completeOutputType, &format, &formatSize );
	CI_ASSERT( hr == S_OK );
	::CoTaskMemFree( format );

	// get seconds:
	::PROPVARIANT durationProp;
	hr = mSourceReader->GetPresentationAttribute( MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &durationProp );
	CI_ASSERT( hr == S_OK );
	LONGLONG duration = durationProp.uhVal.QuadPart; // nanoseconds, divide by 10 million to get seconds. TODO: use PropVariantToInt64

	mSeconds = nanoSecondsToSeconds( duration );

	::PROPVARIANT seekProp;
	hr = mSourceReader->GetPresentationAttribute( MF_SOURCE_READER_MEDIASOURCE, MF_SOURCE_READER_MEDIASOURCE_CHARACTERISTICS, &seekProp );
	CI_ASSERT( hr == S_OK );
	ULONG flags = seekProp.ulVal;
	mCanSeek = ( ( flags & MFMEDIASOURCE_CAN_SEEK ) == MFMEDIASOURCE_CAN_SEEK );
}

size_t SourceFileMediaFoundation::processNextReadSample()
{
	::IMFSample *mediaSample;
	DWORD streamFlags = 0;
	LONGLONG timeStamp;
	HRESULT hr = mSourceReader->ReadSample( MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &streamFlags, &timeStamp, &mediaSample );
	CI_ASSERT( hr == S_OK );

	if( streamFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED ) {
		LOG_E << "type change" << endl;
		return 0;
	}
	if( streamFlags & MF_SOURCE_READERF_ENDOFSTREAM ) {
		LOG_V << "end of file." << endl;
		//mFinished = true;
		return 0;
	}
	if( ! mediaSample ) {
		LOG_V << "no sample." << endl;
		mediaSample->Release();
		return 0;
	}

	auto samplePtr = makeComUnique( mediaSample ); // ???: can this be reused? It is released at every iteration

	DWORD bufferCount;
	hr = samplePtr->GetBufferCount( &bufferCount );
	CI_ASSERT( hr == S_OK );

	CI_ASSERT( bufferCount == 1 ); // just looking out for a file type with more than one buffer.. haven't seen one yet.

	// get the buffer
	::IMFMediaBuffer *mediaBuffer;
	BYTE *audioData = NULL;
	DWORD audioDataLength;

	hr = samplePtr->ConvertToContiguousBuffer( &mediaBuffer );
	hr = mediaBuffer->Lock( &audioData, NULL, &audioDataLength );

	// TODO: copy / de-interleave audioData into buffer, converting to float if necessary
	// - use std::copy or some other more efficient way to do this copy
	size_t numSamplesRead;
	if( mSampleFormat == Format::FLOAT_32 ) {
		float *floatSamples = (float *)audioData;
		numSamplesRead = audioDataLength / 4;
		resizeReadBufferIfNecessary( numSamplesRead );
		memcpy( mReadBuffer.data(), floatSamples, numSamplesRead * sizeof( float ) );
	} else if( mSampleFormat == Format::INT_16 ) {
		INT16 *signedIntSamples = (INT16 *)audioData;
		numSamplesRead = audioDataLength / 2;
		resizeReadBufferIfNecessary( numSamplesRead );
		for( size_t i = 0; i < numSamplesRead; ++i )
			mReadBuffer[i] = (float)signedIntSamples[i] / 32768.0f;
	} else
		CI_ASSERT( 0 && "unknown Format"); 

	hr = mediaBuffer->Unlock();
	CI_ASSERT( hr == S_OK );

	mediaBuffer->Release();

	LOG_V << "num samples read: " << numSamplesRead  << ", timestamp: " << nanoSecondsToSeconds( timeStamp ) << "s" << endl;

	return numSamplesRead;
}

void SourceFileMediaFoundation::resizeReadBufferIfNecessary( size_t requiredSize )
{
	if( requiredSize > mReadBuffer.size() ) {
		LOG_V << "RESIZE buffer to " << requiredSize << endl;
		mReadBuffer.resize( requiredSize );
	}
}

} } } // namespace cinder::audio2::msw
