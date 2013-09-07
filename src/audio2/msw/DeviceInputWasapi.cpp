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

#include "audio2/msw/DeviceInputWasapi.h"
#include "audio2/msw/DeviceManagerWasapi.h"
#include "audio2/msw/util.h"
#include "audio2/audio.h"
#include "audio2/RingBuffer.h"
#include "audio2/Dsp.h"
#include "audio2/CinderAssert.h"
#include "audio2/Debug.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>

using namespace std;

namespace cinder { namespace audio2 { namespace msw {

struct LineInWasapi::Impl {
	Impl() : mNumSamplesBuffered( 0 ) {}
	~Impl() {}

	void initAudioClient( const string &deviceId );
	void initCapture( size_t numFrames );
	void captureAudio();

	std::unique_ptr<::IAudioClient, ComReleaser>			mAudioClient;
	std::unique_ptr<::IAudioCaptureClient, ComReleaser>	mCaptureClient;
	std::unique_ptr<RingBuffer>		mRingBuffer;
	size_t mNumSamplesBuffered, mNumChannels;
};

// converts to 100-nanoseconds
inline ::REFERENCE_TIME samplesToReferenceTime( size_t samples, size_t sampleRate ) {
	return (::REFERENCE_TIME)( (double)samples * 10000000.0 / (double)sampleRate );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceInputWasapi
// ----------------------------------------------------------------------------------------------------

DeviceInputWasapi::DeviceInputWasapi( const std::string &key )
: Device( key )
{

}

DeviceInputWasapi::~DeviceInputWasapi()
{

}

void DeviceInputWasapi::initialize()
{

}

void DeviceInputWasapi::uninitialize()
{

}

void DeviceInputWasapi::start()
{

}

void DeviceInputWasapi::stop()
{

}

// ----------------------------------------------------------------------------------------------------
// MARK: - InputWasapi
// ----------------------------------------------------------------------------------------------------

// TODO: audio client activation should be in Device, maybe audio client should even be in there
// TODO: default block sizes should be set in one place and propagate down the graph
//  - update: it's now at getContext()->getFramesPerBlock() - can get it there
LineInWasapi::LineInWasapi( const DeviceRef &device, const Format &format )
: LineInNode( device, format ), mImpl( new LineInWasapi::Impl() ), mCaptureBlockSize( 1024 )
{
	mDevice = dynamic_pointer_cast<DeviceInputWasapi>( device );
	CI_ASSERT( mDevice );	
}

LineInWasapi::~LineInWasapi()
{
}

void LineInWasapi::initialize()
{
	CI_ASSERT( ! mImpl->mAudioClient );
	mImpl->initAudioClient( mDevice->getKey() );

	size_t sampleRate = getContext()->getSampleRate();

	auto wfx = interleavedFloatWaveFormat( mNumChannels, sampleRate );
	::WAVEFORMATEX *closestMatch;
	HRESULT hr = mImpl->mAudioClient->IsFormatSupported( ::AUDCLNT_SHAREMODE_SHARED, wfx.get(), &closestMatch );
	if( hr == S_OK )
		LOG_V << "requested format is supported." << endl;
	else if( hr == S_FALSE ) {
		CI_ASSERT( closestMatch );
		LOG_V << "cannot use requested format. TODO: use closest" << endl;
	}
	else
		throw AudioFormatExc( "Could not find a suitable format for IAudioCaptureClient" );

	LOG_V << "requested block size: " << mCaptureBlockSize << " frames" << endl;

	::REFERENCE_TIME requestedDuration = samplesToReferenceTime( mCaptureBlockSize, sampleRate );
	hr = mImpl->mAudioClient->Initialize( ::AUDCLNT_SHAREMODE_SHARED, 0, requestedDuration, 0, wfx.get(), NULL ); 
	CI_ASSERT( hr == S_OK );

	UINT32 numFrames;
	hr = mImpl->mAudioClient->GetBufferSize( &numFrames );
	CI_ASSERT( hr == S_OK );

	double captureDurationMs = (double) numFrames * 1000.0 / (double) wfx->nSamplesPerSec;

	mCaptureBlockSize = numFrames;
	mImpl->initCapture( numFrames );

	mInterleavedBuffer = BufferInterleaved( numFrames, mNumChannels );
	
	LOG_V << "numFrames: " << numFrames << ", buffer size: " << mInterleavedBuffer.getSize() << ", actual duration: " << captureDurationMs << "ms" << endl;

	mInitialized = true;
	LOG_V << "complete." << endl;
}

// TODO: consider uninitializing device
void LineInWasapi::uninitialize()
{
	if( ! mInitialized )
		return;

	mImpl->mAudioClient.reset(); // calls Release() on the maintained IAudioClient
	mInitialized = false;
}

void LineInWasapi::start()
{
	if( ! mInitialized ) {
		LOG_E << "not initialized" << endl;
		return;
	}
	
	HRESULT hr = mImpl->mAudioClient->Start();
	CI_ASSERT( hr == S_OK );

	mEnabled = true;
	LOG_V << "started " << mDevice->getName() << endl;
}

void LineInWasapi::stop()
{
	if( ! mInitialized ) {
		LOG_E << "not initialized" << endl;
		return;
	}

	HRESULT hr = mImpl->mAudioClient->Stop();
	CI_ASSERT( hr == S_OK );

	mEnabled = false;
	LOG_V << "stopped " << mDevice->getName() << endl;
}

DeviceRef LineInWasapi::getDevice()
{
	return std::static_pointer_cast<Device>( mDevice );
}

// TODO: set buffer over/under run atomic flags when they occur
void LineInWasapi::process( Buffer *buffer )
{
	mImpl->captureAudio();

	size_t samplesNeeded = buffer->getSize();
	if( mImpl->mNumSamplesBuffered < samplesNeeded ) {
		//LOG_V << "BUFFER UNDERRUN. needed: " << samplesNeeded << ", available: " << mImpl->mNumSamplesBuffered << endl;
		return;
	}

	if( buffer->getNumChannels() == 2 ) {
		size_t numRead = mImpl->mRingBuffer->read( mInterleavedBuffer.getData(), samplesNeeded );
		//CI_ASSERT( numRead == samplesNeeded );
		deinterleaveStereoBuffer( &mInterleavedBuffer, buffer );
	} else {
		size_t numRead = mImpl->mRingBuffer->read( buffer->getData(), samplesNeeded );
		//CI_ASSERT( numRead == samplesNeeded );
	}

	mImpl->mNumSamplesBuffered -= samplesNeeded;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - InputWasapi::Impl
// ----------------------------------------------------------------------------------------------------

void LineInWasapi::Impl::initAudioClient( const string &deviceKey )
{
	CI_ASSERT( ! mAudioClient );

	DeviceManagerWasapi *manager = dynamic_cast<DeviceManagerWasapi *>( DeviceManagerWasapi::instance() );
	CI_ASSERT( manager );

	shared_ptr<::IMMDevice> immDevice = manager->getIMMDevice( deviceKey );

	::IAudioClient *audioClient;
	HRESULT hr = immDevice->Activate( __uuidof(::IAudioClient), CLSCTX_ALL, NULL, (void**)&audioClient );
	CI_ASSERT( hr == S_OK );
	mAudioClient = makeComUnique( audioClient );

	// set default format to match the audio client's defaults

	::WAVEFORMATEX *mixFormat;
	hr = mAudioClient->GetMixFormat( &mixFormat );
	CI_ASSERT( hr == S_OK );

	// TODO: sort out how to specify channels
	// - count
	// - index (ex 4, 5, 8, 9)
	mNumChannels = mixFormat->nChannels;

	LOG_V << "initial mix format samplerate: " << mixFormat->nSamplesPerSec << ", num channels: " << mixFormat->nChannels << endl;
	::CoTaskMemFree( mixFormat );

	LOG_V << "activated audio client" << endl;
}

void LineInWasapi::Impl::initCapture( size_t numFrames ) {
	CI_ASSERT( mAudioClient );

	::IAudioCaptureClient *captureClient;
	HRESULT hr = mAudioClient->GetService( __uuidof(::IAudioCaptureClient), (void**)&captureClient );
	CI_ASSERT( hr == S_OK );
	mCaptureClient = makeComUnique( captureClient );

	mRingBuffer.reset( new RingBuffer( numFrames * mNumChannels ) );
}

void LineInWasapi::Impl::captureAudio()
{
	UINT32 sizeNextPacket;
	HRESULT hr = mCaptureClient->GetNextPacketSize( &sizeNextPacket ); // TODO: treat this accordingly for stereo (2x)
	CI_ASSERT( hr == S_OK );

	while( sizeNextPacket ) {

		if( ( sizeNextPacket * mNumChannels ) > ( mRingBuffer->getSize() - mNumSamplesBuffered ) ) {
			return; // not enough space, we'll read it next time
		}

		BYTE *audioData;
		UINT32 numFramesAvailable; // ???: is this samples or samples * channels? I very well could only be reading half of the samples... enabling mono capture would tell
		DWORD flags;
		HRESULT hr = mCaptureClient->GetBuffer( &audioData, &numFramesAvailable, &flags, NULL, NULL );
		if( hr == AUDCLNT_S_BUFFER_EMPTY ) {
			//LOG_V << "AUDCLNT_S_BUFFER_EMPTY, returning" << endl;
			return;
		}
		else
			CI_ASSERT( hr == S_OK );

		if ( flags & AUDCLNT_BUFFERFLAGS_SILENT ) {
			LOG_V << "silence. TODO: fill buffer with zeros." << endl;
			// ???: ignore this? copying the samples is just about the same as setting to 0
			//fill( mCaptureBuffer.begin(), mCaptureBuffer.end(), 0.0f );
		}
		else {
			float *samples = (float *)audioData;
			size_t numSamples = numFramesAvailable * mNumChannels;
			size_t numDropped = mRingBuffer->write( samples, numSamples );
			//if( numDropped )
				//LOG_V << "BUFFER OVERRUN. dropped: " << numDropped << endl;

			mNumSamplesBuffered += static_cast<size_t>( numSamples );
		}

		hr = mCaptureClient->ReleaseBuffer( numFramesAvailable );
		CI_ASSERT( hr == S_OK );

		hr = mCaptureClient->GetNextPacketSize( &sizeNextPacket );
		CI_ASSERT( hr == S_OK );
	}
}

} } } // namespace cinder::audio2::msw