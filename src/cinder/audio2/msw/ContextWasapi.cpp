/*
 Copyright (c) 2014, The Cinder Project

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

#if( _WIN32_WINNT >= _WIN32_WINNT_VISTA )

#include "cinder/audio2/msw/ContextWasapi.h"
#include "cinder/audio2/msw/DeviceManagerWasapi.h"
#include "cinder/audio2/msw/MswUtil.h"
#include "cinder/audio2/Context.h"
#include "cinder/audio2/dsp/Dsp.h"
#include "cinder/audio2/dsp/RingBuffer.h"
#include "cinder/audio2/dsp/Converter.h"
#include "cinder/audio2/Exception.h"
#include "cinder/audio2/CinderAssert.h"
#include "cinder/audio2/Debug.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>

#include <avrt.h>
#pragma comment(lib, "avrt.lib")

// TODO: should requestedDuration come from Device's frames per block?
#define DEFAULT_AUDIOCLIENT_FRAMES 2048

using namespace std;

namespace cinder { namespace audio2 { namespace msw {

// converts to 100-nanoseconds
inline ::REFERENCE_TIME samplesToReferenceTime( size_t samples, size_t sampleRate ) {
	return (::REFERENCE_TIME)( (double)samples * 10000000.0 / (double)sampleRate );
}

// TODO: rename to RenderImpl and CaptureImpl
struct LineOutWasapi::Impl {
	Impl();
	~Impl();

	void initAudioClient( const DeviceRef &device );
	void initRender( size_t numFrames );
	void increaseThreadPriority();

	unique_ptr<::IAudioClient, ComReleaser>			mAudioClient;
	unique_ptr<::IAudioRenderClient, ComReleaser>	mRenderClient;
	unique_ptr<dsp::RingBuffer>						mRingBuffer;

	::HANDLE      mRenderSamplesReadyEvent, mRenderShouldQuitEvent;

	size_t	mNumSamplesBuffered, mNumChannels;
};

struct LineInWasapi::Impl {
	Impl() : mNumSamplesBuffered( 0 ) {}
	~Impl() {}

	void initAudioClient( const DeviceRef &device );
	void initCapture( size_t numFrames );
	void captureAudio();

	unique_ptr<::IAudioClient, ComReleaser>			mAudioClient;
	unique_ptr<::IAudioCaptureClient, ComReleaser>	mCaptureClient;
	unique_ptr<dsp::RingBuffer>						mRingBuffer;

	size_t	mNumSamplesBuffered, mNumChannels;
};

// ----------------------------------------------------------------------------------------------------
// MARK: - LineOutWasapi
// ----------------------------------------------------------------------------------------------------

LineOutWasapi::LineOutWasapi( const DeviceRef &device, const Format &format )
	: LineOut( device, format ), mImpl( new LineOutWasapi::Impl() ),
	mBlockNumFrames( DEFAULT_AUDIOCLIENT_FRAMES )
{
}

void LineOutWasapi::initialize()
{
	setupProcessWithSumming(); // TODO: do this for all lineouts?

	CI_ASSERT( ! mImpl->mAudioClient );
	mImpl->initAudioClient( mDevice );

	size_t sampleRate = getSampleRate();

	auto wfx = interleavedFloatWaveFormat( sampleRate, mNumChannels );
	::WAVEFORMATEX *closestMatch;
	HRESULT hr = mImpl->mAudioClient->IsFormatSupported( ::AUDCLNT_SHAREMODE_SHARED, wfx.get(), &closestMatch );
	if( hr == S_FALSE ) {
		CI_LOG_E( "cannot use requested format. TODO: use closestMatch" );
		CI_ASSERT( closestMatch );
	}
	else if( hr != S_OK )
		throw AudioFormatExc( "Could not find a suitable format for IAudioClient" );

	DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
	::REFERENCE_TIME requestedDuration = samplesToReferenceTime( mBlockNumFrames, sampleRate );
	hr = mImpl->mAudioClient->Initialize( ::AUDCLNT_SHAREMODE_SHARED, streamFlags, requestedDuration, 0, wfx.get(), NULL ); 
	CI_ASSERT( hr == S_OK );

	mImpl->mAudioClient->SetEventHandle( mImpl->mRenderSamplesReadyEvent );

	UINT32 numFrames;
	hr = mImpl->mAudioClient->GetBufferSize( &numFrames );
	CI_ASSERT( hr == S_OK );

	CI_LOG_V( "requested frames: " << mBlockNumFrames << ", mAudioClient block size: " << numFrames );

	mBlockNumFrames = numFrames; // update capture blocksize with the actual size
	mImpl->initRender( numFrames );

	mInterleavedBuffer = BufferInterleaved( getFramesPerBlock(), mNumChannels );

	mRenderThread = unique_ptr<thread>( new thread( bind( &LineOutWasapi::runRenderThread, this ) ) );
}

void LineOutWasapi::uninitialize()
{
	mImpl->mAudioClient.reset(); // calls Release() on the maintained IAudioClient

	SetEvent( mImpl->mRenderShouldQuitEvent );
	mRenderThread->join();
}

void LineOutWasapi::start()
{
	if( ! mInitialized ) {
		CI_LOG_E( "not initialized" );
		return;
	}

	mEnabled = true;
	HRESULT hr = mImpl->mAudioClient->Start();
	CI_ASSERT( hr == S_OK );
}

void LineOutWasapi::stop()
{
	if( ! mInitialized ) {
		CI_LOG_E( "not initialized" );
		return;
	}

	HRESULT hr = mImpl->mAudioClient->Stop();
	CI_ASSERT( hr == S_OK );

	mEnabled = false;
}

// TODO: reorganize
// - methinks all render() should to is pull, called back from Impl that is running the thread
// - run function initiates capture into buffer, then calls render to do the pulling
void LineOutWasapi::runRenderThread()
{
	mImpl->increaseThreadPriority();

	HANDLE waitEvents[2] = { mImpl->mRenderShouldQuitEvent, mImpl->mRenderSamplesReadyEvent };
	bool running = true;

	while( running ) {
		DWORD waitResult = ::WaitForMultipleObjects( 2, waitEvents, FALSE, INFINITE );
		switch( waitResult ) {
			case WAIT_OBJECT_0 + 0:     // mRenderShouldQuitEvent
				running = false;
				break;
			case WAIT_OBJECT_0 + 1:		// mRenderSamplesReadyEvent
				render();
				break;
			default:
				CI_ASSERT_NOT_REACHABLE();
		}
	}
}

// TODO: sync with Context's mutex
// TODO: move all processProcessing to virtual Context::postProcess() or postRender
void LineOutWasapi::render()
{
	// Request a buffer from IAudioRenderClient

	UINT32 numFramesPadding;
	HRESULT hr = mImpl->mAudioClient->GetCurrentPadding( &numFramesPadding );
	CI_ASSERT( hr == S_OK );

	size_t numWriteFramesAvailable = mBlockNumFrames - numFramesPadding;

	// If there aren't enough buffered samples, pull inputs
	while( mImpl->mNumSamplesBuffered < numWriteFramesAvailable ) {
		mInternalBuffer.zero();
		pullInputs( &mInternalBuffer );

		dsp::interleaveStereoBuffer( &mInternalBuffer, &mInterleavedBuffer );
		mImpl->mRingBuffer->write( mInterleavedBuffer.getData(), mInterleavedBuffer.getSize() );
		mImpl->mNumSamplesBuffered += mDevice->getFramesPerBlock();
	}

	float *renderBuffer;
	hr = mImpl->mRenderClient->GetBuffer( numWriteFramesAvailable, (BYTE **)&renderBuffer );
	CI_ASSERT( hr == S_OK );

	DWORD bufferFlags = 0;
	size_t numSamples = numWriteFramesAvailable * mNumChannels;
	mImpl->mRingBuffer->read( renderBuffer, numSamples );

	hr = mImpl->mRenderClient->ReleaseBuffer( numWriteFramesAvailable, bufferFlags );
	CI_ASSERT( hr == S_OK );
}

LineOutWasapi::Impl::Impl()
	: mNumSamplesBuffered( 0 )
{
	// create render events
	mRenderSamplesReadyEvent = ::CreateEvent( NULL, FALSE, FALSE, NULL );
	CI_ASSERT( mRenderSamplesReadyEvent );

	mRenderShouldQuitEvent = ::CreateEvent( NULL, FALSE, FALSE, NULL );
	CI_ASSERT( mRenderShouldQuitEvent );
}

LineOutWasapi::Impl::~Impl()
{
	if( mRenderSamplesReadyEvent )
		CloseHandle( mRenderSamplesReadyEvent );
	if( mRenderShouldQuitEvent )
		CloseHandle( mRenderShouldQuitEvent );
}

void LineOutWasapi::Impl::initAudioClient( const DeviceRef &device )
{
	CI_ASSERT( ! mAudioClient );

	DeviceManagerWasapi *manager = dynamic_cast<DeviceManagerWasapi *>( Context::deviceManager() );
	CI_ASSERT( manager );

	shared_ptr<::IMMDevice> immDevice = manager->getIMMDevice( device );

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

	::CoTaskMemFree( mixFormat );
}

void LineOutWasapi::Impl::initRender( size_t numFrames ) {
	CI_ASSERT( mAudioClient );

	::IAudioRenderClient *renderClient;
	HRESULT hr = mAudioClient->GetService( __uuidof(::IAudioRenderClient), (void**)&renderClient );
	CI_ASSERT( hr == S_OK );
	mRenderClient = makeComUnique( renderClient );

	mRingBuffer.reset( new dsp::RingBuffer( numFrames * mNumChannels ) );
}

// TODO: also try SetThreadPriority() with THREAD_PRIORITY_TIME_CRITICAL
void LineOutWasapi::Impl::increaseThreadPriority()
{
	DWORD taskIndex = 0;
	HANDLE mmcssHandle = ::AvSetMmThreadCharacteristics( L"Pro Audio", &taskIndex );
	if( ! mmcssHandle )
		CI_LOG_W( "Unable to enable MMCSS for 'Pro Audio', error: " << GetLastError() );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - LineInWasapi
// ----------------------------------------------------------------------------------------------------

LineInWasapi::LineInWasapi( const DeviceRef &device, const Format &format )
	: LineIn( device, format ), mImpl( new LineInWasapi::Impl() ), mBlockNumFrames( DEFAULT_AUDIOCLIENT_FRAMES )
{
}

LineInWasapi::~LineInWasapi()
{
}

void LineInWasapi::initialize()
{
	CI_ASSERT( ! mImpl->mAudioClient );
	mImpl->initAudioClient( mDevice );

	size_t sampleRate = getSampleRate();

	auto wfx = interleavedFloatWaveFormat( sampleRate, mNumChannels );
	::WAVEFORMATEX *closestMatch;
	HRESULT hr = mImpl->mAudioClient->IsFormatSupported( ::AUDCLNT_SHAREMODE_SHARED, wfx.get(), &closestMatch );
	if( hr == S_FALSE ) {
		CI_LOG_E( "cannot use requested format. TODO: use closestMatch" );
		CI_ASSERT( closestMatch );
	}
	else if( hr != S_OK )
		throw AudioFormatExc( "Could not find a suitable format for IAudioClient" );

	::REFERENCE_TIME requestedDuration = samplesToReferenceTime( mBlockNumFrames, sampleRate );
	hr = mImpl->mAudioClient->Initialize( ::AUDCLNT_SHAREMODE_SHARED, 0, requestedDuration, 0, wfx.get(), NULL ); 
	CI_ASSERT( hr == S_OK );

	UINT32 numFrames;
	hr = mImpl->mAudioClient->GetBufferSize( &numFrames );
	CI_ASSERT( hr == S_OK );

	//double captureDurationMs = (double) numFrames * 1000.0 / (double) wfx->nSamplesPerSec;

	mBlockNumFrames = numFrames; // update capture blocksize with the actual size
	mImpl->initCapture( numFrames );

	mInterleavedBuffer = BufferInterleaved( numFrames, mNumChannels );
}

void LineInWasapi::uninitialize()
{
	mImpl->mAudioClient.reset(); // calls Release() on the maintained IAudioClient
}

void LineInWasapi::start()
{
	if( ! mInitialized ) {
		CI_LOG_E( "not initialized" );
		return;
	}

	HRESULT hr = mImpl->mAudioClient->Start();
	CI_ASSERT( hr == S_OK );

	mEnabled = true;
}

void LineInWasapi::stop()
{
	if( ! mInitialized ) {
		CI_LOG_E( "not initialized" );
		return;
	}

	HRESULT hr = mImpl->mAudioClient->Stop();
	CI_ASSERT( hr == S_OK );

	mEnabled = false;
}

uint64_t LineInWasapi::getLastUnderrun()
{
	return 0; // TODO
}

uint64_t LineInWasapi::getLastOverrun()
{
	return 0; // TODO
}

// TODO: set buffer over/under run atomic flags when they occur
void LineInWasapi::process( Buffer *buffer )
{
	mImpl->captureAudio();

	size_t samplesNeeded = buffer->getSize();
	if( mImpl->mNumSamplesBuffered < samplesNeeded )
		return;

	if( buffer->getNumChannels() == 2 ) {
		size_t numRead = mImpl->mRingBuffer->read( mInterleavedBuffer.getData(), samplesNeeded );
		dsp::deinterleaveStereoBuffer( &mInterleavedBuffer, buffer );
	} else
		size_t numRead = mImpl->mRingBuffer->read( buffer->getData(), samplesNeeded );

	mImpl->mNumSamplesBuffered -= samplesNeeded;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - InputWasapi::Impl
// ----------------------------------------------------------------------------------------------------

void LineInWasapi::Impl::initAudioClient( const DeviceRef &device )
{
	CI_ASSERT( ! mAudioClient );

	DeviceManagerWasapi *manager = dynamic_cast<DeviceManagerWasapi *>( Context::deviceManager() );
	CI_ASSERT( manager );

	shared_ptr<::IMMDevice> immDevice = manager->getIMMDevice( device );

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

	::CoTaskMemFree( mixFormat );
}

void LineInWasapi::Impl::initCapture( size_t numFrames ) {
	CI_ASSERT( mAudioClient );

	::IAudioCaptureClient *captureClient;
	HRESULT hr = mAudioClient->GetService( __uuidof(::IAudioCaptureClient), (void**)&captureClient );
	CI_ASSERT( hr == S_OK );
	mCaptureClient = makeComUnique( captureClient );

	mRingBuffer.reset( new dsp::RingBuffer( numFrames * mNumChannels ) );
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
		if( hr == AUDCLNT_S_BUFFER_EMPTY )
			return;
		else
			CI_ASSERT( hr == S_OK );

		if ( flags & AUDCLNT_BUFFERFLAGS_SILENT ) {
			CI_LOG_V( "silence. TODO: fill buffer with zeros." );
			// ???: ignore this? copying the samples is just about the same as setting to 0
			//fill( mCaptureBuffer.begin(), mCaptureBuffer.end(), 0.0f );
		}
		else {
			float *samples = (float *)audioData;
			size_t numSamples = numFramesAvailable * mNumChannels;
			size_t numDropped = mRingBuffer->write( samples, numSamples );
			//if( numDropped )
			//CI_LOG_V( "BUFFER OVERRUN. dropped: " << numDropped );

			mNumSamplesBuffered += static_cast<size_t>( numSamples );
		}

		hr = mCaptureClient->ReleaseBuffer( numFramesAvailable );
		CI_ASSERT( hr == S_OK );

		hr = mCaptureClient->GetNextPacketSize( &sizeNextPacket );
		CI_ASSERT( hr == S_OK );
	}
}

// ----------------------------------------------------------------------------------------------------
// MARK: - ContextWasapi
// ----------------------------------------------------------------------------------------------------

LineOutRef ContextWasapi::createLineOut( const DeviceRef &device, const Node::Format &format )
{
	return makeNode( new LineOutWasapi( device, format ) );
}

LineInRef ContextWasapi::createLineIn( const DeviceRef &device, const Node::Format &format )
{
	return makeNode( new LineInWasapi( device, format ) );
}

} } } // namespace cinder::audio2::msw

#endif // ( _WIN32_WINNT >= _WIN32_WINNT_VISTA )
