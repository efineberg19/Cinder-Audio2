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

#include "cinder/audio2/msw/FileMediaFoundation.h"
#include "cinder/audio2/Debug.h"

#include <mfidl.h>
#include <mfapi.h>
#include <mfreadwrite.h>
#include <propvarutil.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")

// TODO: try to minimize the number of copies between IMFSourceReader::ReadSample's IMFSample and loading audio2::Buffer
// - currently uses an intermediate vector<float>
// - want to have minimal copies when loading a buffer for BufferPlayerNode, so
// - this is on hold until audio2::Converter is re-addressed

using namespace std;
using namespace ci;

namespace cinder { namespace audio2 { namespace msw {

namespace {

inline double nanoSecondsToSeconds( LONGLONG ns )
{
	return (double)ns / 10000000.0;
} 

inline LONGLONG secondsToNanoSeconds( double seconds )
{
	return (LONGLONG)seconds * 10000000;
}

struct MfInitializer {
	MfInitializer()
	{
		HRESULT hr = ::MFStartup( MF_VERSION ); // TODO: try passing in MFSTARTUP_LITE (no sockets) and see if load is faster
		CI_ASSERT( hr == S_OK );
	}

	~MfInitializer()
	{
		HRESULT hr = ::MFShutdown();
		CI_ASSERT( hr == S_OK );
	}
};

} // anonymous namespace

// ----------------------------------------------------------------------------------------------------
// MARK: - SourceFileMediaFoundation
// ----------------------------------------------------------------------------------------------------

SourceFileMediaFoundation::SourceFileMediaFoundation()
	: SourceFile(), mCanSeek( false ), mSeconds( 0 ), mReadBufferPos( 0 ), mFramesRemainingInReadBuffer( 0 )
{
}

SourceFileMediaFoundation::SourceFileMediaFoundation( const DataSourceRef &dataSource )
	: SourceFile(), mDataSource( dataSource ), mCanSeek( false ), mSeconds( 0 ), mReadBufferPos( 0 ), mFramesRemainingInReadBuffer( 0 )
{
	initMediaFoundation();
	initReader();

	LOG_V( "complete. total seconds: " << mSeconds << ", frames: " << mNumFrames << ", can seek: " << mCanSeek );
}

SourceFileRef SourceFileMediaFoundation::clone() const
{
	shared_ptr<SourceFileMediaFoundation> result( new SourceFileMediaFoundation );
	result->mDataSource = mDataSource;
	result->initReader();

	return result;
}

SourceFileMediaFoundation::~SourceFileMediaFoundation()
{
	mSourceReader.reset(); // needs to be released before MfInitializer goes out of scope
}

size_t SourceFileMediaFoundation::performRead( Buffer *buffer, size_t bufferFrameOffset, size_t numFramesNeeded )
{
	CI_ASSERT( buffer->getNumFrames() >= bufferFrameOffset + numFramesNeeded );

	size_t readCount = 0;
	while( readCount < numFramesNeeded ) {

		// first drain any frames that were previously read from an IMFSample
		if( mFramesRemainingInReadBuffer ) {
			size_t remainingToDrain = min( mFramesRemainingInReadBuffer, numFramesNeeded );

			// TODO: can move this type of copy to Buffer.h? it is used all over the place
			for( size_t ch = 0; ch < mNativeNumChannels; ch++ ) {
				float *readChannel = mReadBuffer.getChannel( ch ) + mReadBufferPos;
				float *resultChannel = buffer->getChannel( ch );
				memcpy( resultChannel + readCount, readChannel, remainingToDrain * sizeof( float ) );
			}

			mReadBufferPos += remainingToDrain;
			mFramesRemainingInReadBuffer -= remainingToDrain;
			readCount += remainingToDrain;
			continue;
		}

		CI_ASSERT( ! mFramesRemainingInReadBuffer );

		mReadBufferPos = 0;
		size_t outNumFrames = processNextReadSample();
		if( ! outNumFrames )
			break;

		// if the IMFSample num frames is over the specified buffer size, 
		// record how many samples are left over and use up what was asked for.
		if( outNumFrames + readCount > numFramesNeeded ) {
			mFramesRemainingInReadBuffer = outNumFrames + readCount - numFramesNeeded;
			outNumFrames = numFramesNeeded - readCount;
		}

		size_t offset = bufferFrameOffset + readCount;
		for( size_t ch = 0; ch < mNativeNumChannels; ch++ ) {
			float *readChannel = mReadBuffer.getChannel( ch );
			float *resultChannel = buffer->getChannel( ch );
			memcpy( resultChannel + readCount, readChannel, outNumFrames * sizeof( float ) );
		}

		mReadBufferPos += outNumFrames;
		readCount += outNumFrames;
	}

	return readCount;
}

void SourceFileMediaFoundation::performSeek( size_t readPositionFrames )
{
	if( ! mCanSeek ) {
		LOG_E( "cannot seek." );
		return;
	}

	mReadBufferPos = mFramesRemainingInReadBuffer = 0;

	double positionSeconds = (double)readPositionFrames / (double)mSampleRate;
	if( positionSeconds > mSeconds ) {
		LOG_E( "cannot seek beyond end of file (" << positionSeconds << "s)." );
		return;
	}

	LONGLONG position = secondsToNanoSeconds( positionSeconds );
	PROPVARIANT seekVar;
	HRESULT hr = ::InitPropVariantFromInt64( position, &seekVar );
	CI_ASSERT( hr == S_OK );
	hr = mSourceReader->SetCurrentPosition( GUID_NULL, seekVar );
	CI_ASSERT( hr == S_OK );
	hr = PropVariantClear( &seekVar );
	CI_ASSERT( hr == S_OK );
}

void SourceFileMediaFoundation::initMediaFoundation()
{
	static unique_ptr<MfInitializer> sMfInitializer;
	if( ! sMfInitializer )
		sMfInitializer = unique_ptr<MfInitializer>( new MfInitializer() );
}

// TODO: test setting MF_LOW_LATENCY attribute
void SourceFileMediaFoundation::initReader()
{
	CI_ASSERT( mDataSource );
	mFramesRemainingInReadBuffer = 0;

	::IMFAttributes *attributes;
	HRESULT hr = ::MFCreateAttributes( &attributes, 1 );
	CI_ASSERT( hr == S_OK );
	auto attributesPtr = makeComUnique( attributes );

	::IMFSourceReader *sourceReader;

	if( mDataSource->isFilePath() ) {
		hr = ::MFCreateSourceReaderFromURL( mDataSource->getFilePath().wstring().c_str(), attributesPtr.get(), &sourceReader );
		CI_ASSERT( hr == S_OK );
	}
	else {
		mComIStream = makeComUnique( new ComIStream( mDataSource->createStream() ) );
		::IMFByteStream *byteStream;
		hr = ::MFCreateMFByteStreamOnStream( mComIStream.get(), &byteStream );
		CI_ASSERT( hr == S_OK );
		mByteStream = makeComUnique( byteStream );

		hr = ::MFCreateSourceReaderFromByteStream( byteStream, attributesPtr.get(), &sourceReader );
		CI_ASSERT( hr == S_OK );
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

	GUID outputSubType = MFAudioFormat_PCM; // default to PCM, upgrade if we can.
	mSampleFormat = Format::INT_16;
	mBytesPerSample = 2;
	LOG_V( "native bytes per sample: " << mBytesPerSample );

	if( fileFormat->wBitsPerSample == 32 ) {
		mSampleFormat = Format::FLOAT_32;
		mBytesPerSample = 4;
		outputSubType = MFAudioFormat_Float;
	}

	mNumChannels = mNativeNumChannels = fileFormat->nChannels;
	mSampleRate = mNativeSampleRate = fileFormat->nSamplesPerSec;

	LOG_V( "file channels: " << mNumChannels << ", samplerate: " << mSampleRate );

	::CoTaskMemFree( fileFormat );

	mReadBuffer.setSize( mMaxFramesPerRead, mNativeNumChannels );

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
	// - format seems to always have a reliable bits per sample
	::IMFMediaType *completeOutputType;
	hr = mSourceReader->GetCurrentMediaType( MF_SOURCE_READER_FIRST_AUDIO_STREAM, &completeOutputType );
	CI_ASSERT( hr == S_OK );

	::WAVEFORMATEX *format;
	hr = ::MFCreateWaveFormatExFromMFMediaType( completeOutputType, &format, &formatSize );
	CI_ASSERT( hr == S_OK );
	::CoTaskMemFree( format );

	// get seconds:
	::PROPVARIANT durationProp;
	hr = mSourceReader->GetPresentationAttribute( MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &durationProp );
	CI_ASSERT( hr == S_OK );
	LONGLONG duration = durationProp.uhVal.QuadPart;
	
	mSeconds = nanoSecondsToSeconds( duration );
	mNumFrames = mFileNumFrames = size_t( mSeconds * (double)mSampleRate );

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
		LOG_E( "type change" );
		return 0;
	}
	if( streamFlags & MF_SOURCE_READERF_ENDOFSTREAM ) {
		LOG_V( "end of file." );
		return 0;
	}
	if( ! mediaSample ) {
		LOG_V( "no sample." );
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

	size_t numChannels = mNativeNumChannels;
	size_t numFramesRead = audioDataLength / ( mBytesPerSample * numChannels );

	mReadBuffer.setNumFrames( numFramesRead );

	if( mSampleFormat == Format::FLOAT_32 ) {
		float *floatSamples = (float *)audioData;
		if( numChannels == 1) {
			memcpy( mReadBuffer.getData(), floatSamples, numFramesRead * sizeof( float ) );
		}
		else {
			for( size_t ch = 0; ch < numChannels; ch++ ) {
				float *channel = mReadBuffer.getChannel( ch );
				for( size_t i = 0; i < numFramesRead; i++ )
					channel[i] = floatSamples[numChannels * i + ch];
			}
		}
	}
	else if( mSampleFormat == Format::INT_16 ) {
		INT16 *signedIntSamples = (INT16 *)audioData;
		if( numChannels == 1 ) {
			float *data = mReadBuffer.getData();
			for( size_t i = 0; i < numFramesRead; ++i )
				data[i] = (float)signedIntSamples[i] / 32768.0f;
		}
		else {
			for( size_t ch = 0; ch < numChannels; ch++ ) {
				float *channel = mReadBuffer.getChannel( ch );
				for( size_t i = 0; i < numFramesRead; i++ )
					channel[i] = (float)signedIntSamples[numChannels * i + ch] / 32768.0f;
			}
		}
	}
	else
		CI_ASSERT( 0 && "unknown Format"); 

	hr = mediaBuffer->Unlock();
	CI_ASSERT( hr == S_OK );

	mediaBuffer->Release();
	return numFramesRead;
}

} } } // namespace cinder::audio2::msw
