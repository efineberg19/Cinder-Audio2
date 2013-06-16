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

namespace audio2 { namespace msw {

// ----------------------------------------------------------------------------------------------------
// MARK: - SourceFileMediaFoundation
// ----------------------------------------------------------------------------------------------------

// TODO: consider moving MFStartup / MFShutdown to a singleton
// - reference count the current number of SourceFileMediaFoundation objects
// - call shutdown at zero

// TODO: test setting MF_LOW_LATENCY attribute

SourceFileMediaFoundation::SourceFileMediaFoundation( ci::DataSourceRef dataSource, size_t numChannels, size_t sampleRate )
: SourceFile( dataSource, numChannels, sampleRate ), mReadPos( 0 ), mCanSeek( false ), mSeconds( 0.0f )
{
	 HRESULT hr = ::MFStartup( MF_VERSION ); // TODO: try passing in MFSTARTUP_NOSOCKET and see if load is faster
	 CI_ASSERT( hr == S_OK );

	 ::IMFAttributes *attributes;
	 hr = ::MFCreateAttributes( &attributes, 1 );
	 CI_ASSERT( hr == S_OK );
	 auto attributesPtr = makeComUnique( attributes );

	 ::IMFSourceReader *sourceReader;
	 ::LPCWSTR filePath = static_cast<::LPCWSTR>( dataSource->getFilePath().c_str() );
	 hr = ::MFCreateSourceReaderFromURL( filePath, attributesPtr.get(), &sourceReader );
	 CI_ASSERT( hr == S_OK );
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

	 CoTaskMemFree( fileFormat );

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
	 ::IMFMediaType *completeOutputType;
	 hr = mSourceReader->GetCurrentMediaType( MF_SOURCE_READER_FIRST_AUDIO_STREAM, &completeOutputType );
	 CI_ASSERT( hr == S_OK );

	 ::WAVEFORMATEX *format;
	 hr = MFCreateWaveFormatExFromMFMediaType( completeOutputType, &format, &formatSize );
	 CI_ASSERT( hr == S_OK );

	 //mFormat = *format;

	 // if the format was 16bit, we're going to convert to floats so we update to reflect that
	 //if( mSampleFormat == Format::INT_16 ) {
		// mFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
		// mFormat.wBitsPerSample = 32; // we converted to float in SoundReader
		// mFormat.nBlockAlign = mFormat.nChannels * mFormat.wBitsPerSample / 8;
		// mFormat.nAvgBytesPerSec = mFormat.nSamplesPerSec * mFormat.nBlockAlign;
	 //}

	 storeAttributes();

	// TODO: need mNumFrames for pre-buffering
	//mNumFrames = static_cast<size_t>( numFrames );

	 CI_ASSERT( ! mNumChannels && ! mSampleFormat ); // TODO: figure out how to convert MF
	if( ! mNumChannels )
		mNumChannels = mFileNumChannels;
	if( ! mSampleRate )
		mSampleRate = mFileSampleRate;


	 ::CoTaskMemFree( format );
	 LOG_V << "complete." << endl;
}

SourceFileMediaFoundation::~SourceFileMediaFoundation()
{
	HRESULT hr = ::MFShutdown();
	CI_ASSERT( hr == S_OK );
}
size_t SourceFileMediaFoundation::read( Buffer *buffer )
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
		return 0;
	}
	auto samplePtr = makeComUnique( mediaSample ); // ???: can this be reused? It is released at every iteration

	DWORD bufferCount;
	hr = samplePtr->GetBufferCount( &bufferCount );
	CI_ASSERT( hr == S_OK );

	CI_ASSERT( bufferCount == 1 ); // just looking out for a file type with more than one buffer.. haven't seen one yet.

	// get the buffer
	DWORD currentLength;
	::IMFMediaBuffer *mediaBuffer;
	BYTE *audioData = NULL;

	hr = samplePtr->ConvertToContiguousBuffer( &mediaBuffer );
	hr = mediaBuffer->Lock( &audioData, NULL, &currentLength );

	// TODO NEXT: copy / deinterleave audioData into buffer, converting to float if necessary
	// - use std::copy or some other more efficient way to do this copy
	//if( mSampleFormat == Format::FLOAT_32 ) {
	//	currentLength /= 4;
	//	resizeIfNecessary( buffer, currentLength );
	//	float *floatSamples = (float *)audioData;
	//	for( DWORD i = 0; i < currentLength; ++i ) {
	//		buffer->at( i ) = floatSamples[i];
	//	}
	//} else if( mSampleFormat == Format::INT_16 ) {
	//	currentLength /= 2;
	//	resizeIfNecessary( buffer, currentLength );
	//	INT16 *signedIntSamples = (INT16 *)audioData;
	//	for( DWORD i = 0; i < currentLength; ++i ) {
	//		buffer->at( i ) = (float)signedIntSamples[i] / 32768.0f;
	//	}
	//} else {
	//	CI_ASSERT( 0 && "unknown Format"); 
	//	return 0;
	//}

	hr = mediaBuffer->Unlock();
	CI_ASSERT( hr == S_OK );

	mediaBuffer->Release();

	audioData = NULL;

	//LOG_V << "num samples read: " << numSamplesRead  << ", timestamp: " << (float)timeStamp / 10000000.0f << "s" << endl;

	return currentLength;
}

BufferRef SourceFileMediaFoundation::loadBuffer()
{
	//if( mReadPos != 0 )
	//	seek( 0 );
	//
	//BufferRef result( new Buffer( mNumChannels, mNumFrames ) );

	//size_t currReadPos = 0;
	//while( currReadPos < mNumFrames ) {
	//	UInt32 frameCount = std::min( mNumFrames - currReadPos, mNumFramesPerRead );

 //       for( int i = 0; i < mNumChannels; i++ ) {
 //           mBufferList->mBuffers[i].mDataByteSize = frameCount * sizeof( float );
 //           mBufferList->mBuffers[i].mData = &result->getChannel( i )[currReadPos];
 //       }

	//	OSStatus status = ::ExtAudioFileRead( mExtAudioFile.get(), &frameCount, mBufferList.get() );
	//	CI_ASSERT( status == noErr );

 //       currReadPos += frameCount;
	//}
	//return result;
	return BufferRef();
}


inline float nanoSecondsToSeconds( LONGLONG ns )
{
	return (float)ns / 10000000.0f;
} 

inline LONGLONG secondsToNanoSeconds( float seconds )
{
	return (LONGLONG)seconds * 10000000;
} 

void SourceFileMediaFoundation::seek( size_t readPosition )
{
	//if( readPosition >= mNumFrames )
	//	return;

	//OSStatus status = ::ExtAudioFileSeek( mExtAudioFile.get(), readPosition );
	//CI_ASSERT( status == noErr );

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

void SourceFileMediaFoundation::updateOutputFormat()
{
	//::AudioStreamBasicDescription outputFormat = audio2::cocoa::nonInterleavedFloatABSD( mNumChannels, mSampleRate );
	//OSStatus status = ::ExtAudioFileSetProperty( mExtAudioFile.get(), kExtAudioFileProperty_ClientDataFormat, sizeof( outputFormat ), &outputFormat );
	//CI_ASSERT( status == noErr );

	//// numFrames will be updated at read time
	//mBufferList = audio2::cocoa::createNonInterleavedBufferList( mNumChannels, 0 );
}

// TODO: query number of streams, so we know if file has more than one.
void SourceFileMediaFoundation::storeAttributes()
{
	// get seconds:
	::PROPVARIANT prop;
	const float kTenMillion = 10000000.0f;

	HRESULT hr = mSourceReader->GetPresentationAttribute( MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &prop );
	CI_ASSERT( hr == S_OK );
	LONGLONG duration = prop.uhVal.QuadPart; // nanoseconds, divide by 10 million to get seconds. TODO: use PropVariantToInt64

	mSeconds = nanoSecondsToSeconds( duration );
	::PropVariantClear( &prop );
	LOG_V << "total seconds: " << mSeconds << endl;

	// see if seek is supported
	PropVariantInit( &prop );

	hr = mSourceReader->GetPresentationAttribute( MF_SOURCE_READER_MEDIASOURCE, MF_SOURCE_READER_MEDIASOURCE_CHARACTERISTICS, &prop );
	CI_ASSERT( hr == S_OK );
	ULONG flags = prop.ulVal;
	mCanSeek = ( ( flags & MFMEDIASOURCE_CAN_SEEK ) == MFMEDIASOURCE_CAN_SEEK );

	LOG_V << "can seek: " << mCanSeek << endl;
}

} } // namespace audio2::msw
