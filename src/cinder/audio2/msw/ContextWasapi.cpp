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
// - seems like this is fixed at 10ms in shared mode. (896 samples @ stereo 44100 s/r) 
#define DEFAULT_AUDIOCLIENT_FRAMES 1024

using namespace std;

namespace cinder { namespace audio2 { namespace msw {

// converts to 100-nanoseconds
inline ::REFERENCE_TIME samplesToReferenceTime( size_t samples, size_t sampleRate ) {
	return (::REFERENCE_TIME)( (double)samples * 10000000.0 / (double)sampleRate );
}

struct RenderImplWasapi {
	RenderImplWasapi( LineOutWasapi *lineOut );
	~RenderImplWasapi();

	void init();
	void uninit();
	void initAudioClient();
	void initRenderClient();
	void runRenderThread();
	void renderAudio();
	void increaseThreadPriority();

	static DWORD __stdcall renderThreadEntryPoint( LPVOID Context );

	unique_ptr<::IAudioClient, ComReleaser>			mAudioClient;
	unique_ptr<::IAudioRenderClient, ComReleaser>	mRenderClient;
	unique_ptr<dsp::RingBuffer>						mRingBuffer;

	::HANDLE	mRenderSamplesReadyEvent, mRenderShouldQuitEvent;
	::HANDLE    mRenderThread;

	size_t	mNumFramesBuffered, mNumRenderFrames, mNumChannels;
	LineOutWasapi*	mLineOut; // weak pointer to parent
};

struct CaptureImplWasapi {
	CaptureImplWasapi() : mNumSamplesBuffered( 0 ) {}

	void initAudioClient( const DeviceRef &device );
	void initCapture( size_t numFrames );
	void captureAudio();

	unique_ptr<::IAudioClient, ComReleaser>			mAudioClient;
	unique_ptr<::IAudioCaptureClient, ComReleaser>	mCaptureClient;
	unique_ptr<dsp::RingBuffer>						mRingBuffer;

	size_t	mNumSamplesBuffered, mNumChannels;
};

// ----------------------------------------------------------------------------------------------------
// MARK: - RenderImplWasapi
// ----------------------------------------------------------------------------------------------------

RenderImplWasapi::RenderImplWasapi( LineOutWasapi *lineOut )
	: mLineOut( lineOut ), mNumRenderFrames( DEFAULT_AUDIOCLIENT_FRAMES ), mNumFramesBuffered( 0 ), mNumChannels( 0 )
{
	// create render events
	mRenderSamplesReadyEvent = ::CreateEvent( NULL, FALSE, FALSE, NULL );
	CI_ASSERT( mRenderSamplesReadyEvent );

	mRenderShouldQuitEvent = ::CreateEvent( NULL, FALSE, FALSE, NULL );
	CI_ASSERT( mRenderShouldQuitEvent );
}

RenderImplWasapi::~RenderImplWasapi()
{
	if( mRenderSamplesReadyEvent ) {
		BOOL success = ::CloseHandle( mRenderSamplesReadyEvent );
		CI_ASSERT( success );
	}
	if( mRenderShouldQuitEvent ) {
		BOOL success = ::CloseHandle( mRenderShouldQuitEvent );
		CI_ASSERT( success );
	}
}

void RenderImplWasapi::init()
{
	initAudioClient();
	initRenderClient();
}

void RenderImplWasapi::uninit()
{
	// signal quit event and join the render thread once it completes.
	BOOL success = ::SetEvent( mRenderShouldQuitEvent );
	CI_ASSERT( success );

	::WaitForSingleObject( mRenderThread, INFINITE );
	CloseHandle( mRenderThread );
	mRenderThread = NULL;

	// Release() IAudioRenderClient IAudioClient
	mRenderClient.reset();
	mAudioClient.reset();

	// reset should quit event in case we are re-init'ed.
	success = ::ResetEvent( mRenderShouldQuitEvent );
	CI_ASSERT( success );
}

void RenderImplWasapi::initAudioClient()
{
	CI_ASSERT( ! mAudioClient );

	DeviceManagerWasapi *manager = dynamic_cast<DeviceManagerWasapi *>( Context::deviceManager() );
	CI_ASSERT( manager );

	auto device =  mLineOut->getDevice();
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

	auto wfx = interleavedFloatWaveFormat( device->getSampleRate(), mNumChannels );
	::WAVEFORMATEX *closestMatch;
	hr = mAudioClient->IsFormatSupported( ::AUDCLNT_SHAREMODE_SHARED, wfx.get(), &closestMatch );
	if( hr == S_FALSE ) {
		CI_LOG_E( "cannot use requested format. TODO: use closestMatch" );
		CI_ASSERT( closestMatch );
	}
	else if( hr != S_OK )
		throw AudioFormatExc( "Could not find a suitable format for IAudioClient" );

	DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
	::REFERENCE_TIME requestedDuration = samplesToReferenceTime( mNumRenderFrames, wfx->nSamplesPerSec );
	hr = mAudioClient->Initialize( ::AUDCLNT_SHAREMODE_SHARED, streamFlags, requestedDuration, 0, wfx.get(), NULL ); 
	CI_ASSERT( hr == S_OK );

	mAudioClient->SetEventHandle( mRenderSamplesReadyEvent );

	UINT32 numFrames;
	hr = mAudioClient->GetBufferSize( &numFrames );
	CI_ASSERT( hr == S_OK );

	CI_LOG_V( "requested frames: " << mNumRenderFrames << ", mAudioClient block size: " << numFrames );

	mNumRenderFrames = numFrames; // update capture blocksize with the actual size
}

void RenderImplWasapi::initRenderClient()
{
	CI_ASSERT( mAudioClient );

	::IAudioRenderClient *renderClient;
	HRESULT hr = mAudioClient->GetService( __uuidof(::IAudioRenderClient), (void**)&renderClient );
	CI_ASSERT( hr == S_OK );
	mRenderClient = makeComUnique( renderClient );

	mRingBuffer.reset( new dsp::RingBuffer( mNumRenderFrames * mNumChannels ) );

	//mRenderThread = unique_ptr<thread>( new thread( bind( &RenderImplWasapi::runRenderThread, this ) ) );

	mRenderThread = ::CreateThread( NULL, 0, renderThreadEntryPoint, this, 0, NULL );
	CI_ASSERT( mRenderThread );
}

// static
DWORD RenderImplWasapi::renderThreadEntryPoint( LPVOID Context )
{
	RenderImplWasapi *renderer = static_cast<RenderImplWasapi *>( Context );
	renderer->runRenderThread();

	return 0;
}

void RenderImplWasapi::runRenderThread()
{
	increaseThreadPriority();

	HANDLE waitEvents[2] = { mRenderShouldQuitEvent, mRenderSamplesReadyEvent };
	bool running = true;

	while( running ) {
		DWORD waitResult = ::WaitForMultipleObjects( 2, waitEvents, FALSE, INFINITE );
		switch( waitResult ) {
			case WAIT_OBJECT_0 + 0:     // mRenderShouldQuitEvent
				running = false;
				//::CloseHandle( mRenderSamplesReadyEvent );
				break;
			case WAIT_OBJECT_0 + 1:		// mRenderSamplesReadyEvent
				renderAudio();
				break;
			default:
				CI_ASSERT_NOT_REACHABLE();
		}
	}
}

void RenderImplWasapi::renderAudio()
{
	// the current padding represents the number of frames queued on the audio endpoint, waiting to be sent to hardware.
	UINT32 numFramesPadding;
	HRESULT hr = mAudioClient->GetCurrentPadding( &numFramesPadding );
	CI_ASSERT( hr == S_OK );

	size_t numWriteFramesAvailable = mNumRenderFrames - numFramesPadding;

	while( mNumFramesBuffered < numWriteFramesAvailable )
		mLineOut->renderInputs();

	float *renderBuffer;
	hr = mRenderClient->GetBuffer( numWriteFramesAvailable, (BYTE **)&renderBuffer );
	CI_ASSERT( hr == S_OK );

	DWORD bufferFlags = 0;
	size_t numReadSamples = numWriteFramesAvailable * mNumChannels;
	bool readSuccess = mRingBuffer->read( renderBuffer, numReadSamples );
	CI_ASSERT( readSuccess );
	mNumFramesBuffered -= numWriteFramesAvailable;

	hr = mRenderClient->ReleaseBuffer( numWriteFramesAvailable, bufferFlags );
	CI_ASSERT( hr == S_OK );
}

// This uses the "Multimedia Class Scheduler Service" (MMCSS) to increase the priority of the current thread.
// The priority increase can be seen in the threads debugger, it should have Priority = "Time Critical"
void RenderImplWasapi::increaseThreadPriority()
{
	DWORD taskIndex = 0;
	::HANDLE avrtHandle = ::AvSetMmThreadCharacteristics( L"Pro Audio", &taskIndex );
	if( ! avrtHandle )
		CI_LOG_W( "Unable to enable MMCSS for 'Pro Audio', error: " << GetLastError() );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - CaptureImplWasapi
// ----------------------------------------------------------------------------------------------------

void CaptureImplWasapi::initAudioClient( const DeviceRef &device )
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

void CaptureImplWasapi::initCapture( size_t numFrames ) {
	CI_ASSERT( mAudioClient );

	::IAudioCaptureClient *captureClient;
	HRESULT hr = mAudioClient->GetService( __uuidof(::IAudioCaptureClient), (void**)&captureClient );
	CI_ASSERT( hr == S_OK );
	mCaptureClient = makeComUnique( captureClient );

	mRingBuffer.reset( new dsp::RingBuffer( numFrames * mNumChannels ) );
}

void CaptureImplWasapi::captureAudio()
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
// MARK: - LineOutWasapi
// ----------------------------------------------------------------------------------------------------

LineOutWasapi::LineOutWasapi( const DeviceRef &device, const Format &format )
	: LineOut( device, format ), mRenderImpl( new RenderImplWasapi( this ) )
{
}

void LineOutWasapi::initialize()
{
	setupProcessWithSumming();
	mInterleavedBuffer = BufferInterleaved( getFramesPerBlock(), mNumChannels );

	mRenderImpl->init();
}

void LineOutWasapi::uninitialize()
{
	mRenderImpl->uninit();
}

void LineOutWasapi::start()
{
	if( ! mInitialized ) {
		CI_LOG_E( "not initialized" );
		return;
	}

	mEnabled = true;
	HRESULT hr = mRenderImpl->mAudioClient->Start();
	CI_ASSERT( hr == S_OK );
}

void LineOutWasapi::stop()
{
	if( ! mInitialized ) {
		CI_LOG_E( "not initialized" );
		return;
	}

	HRESULT hr = mRenderImpl->mAudioClient->Stop();
	CI_ASSERT( hr == S_OK );

	mEnabled = false;
}

void LineOutWasapi::renderInputs()
{
	lock_guard<mutex> lock( getContext()->getMutex() );

	// verify context still exists, since its destructor may have been holding the lock
	auto ctx = getContext();
	if( ! ctx )
		return;

	mInternalBuffer.zero();
	pullInputs( &mInternalBuffer );

	if( checkNotClipping() )
		mInternalBuffer.zero();

	dsp::interleaveStereoBuffer( &mInternalBuffer, &mInterleavedBuffer );
	bool writeSuccess = mRenderImpl->mRingBuffer->write( mInterleavedBuffer.getData(), mInterleavedBuffer.getSize() );
	CI_ASSERT( writeSuccess );
	mRenderImpl->mNumFramesBuffered += mInterleavedBuffer.getNumFrames();

	postProcess();
}

// ----------------------------------------------------------------------------------------------------
// MARK: - LineInWasapi
// ----------------------------------------------------------------------------------------------------

LineInWasapi::LineInWasapi( const DeviceRef &device, const Format &format )
	: LineIn( device, format ), mCaptureImpl( new CaptureImplWasapi() ), mBlockNumFrames( DEFAULT_AUDIOCLIENT_FRAMES )
{
}

LineInWasapi::~LineInWasapi()
{
}

void LineInWasapi::initialize()
{
	CI_ASSERT( ! mCaptureImpl->mAudioClient );
	mCaptureImpl->initAudioClient( mDevice );

	size_t sampleRate = getSampleRate();

	auto wfx = interleavedFloatWaveFormat( sampleRate, mNumChannels );
	::WAVEFORMATEX *closestMatch;
	HRESULT hr = mCaptureImpl->mAudioClient->IsFormatSupported( ::AUDCLNT_SHAREMODE_SHARED, wfx.get(), &closestMatch );
	if( hr == S_FALSE ) {
		CI_LOG_E( "cannot use requested format. TODO: use closestMatch" );
		CI_ASSERT( closestMatch );
	}
	else if( hr != S_OK )
		throw AudioFormatExc( "Could not find a suitable format for IAudioClient" );

	::REFERENCE_TIME requestedDuration = samplesToReferenceTime( mBlockNumFrames, sampleRate );
	hr = mCaptureImpl->mAudioClient->Initialize( ::AUDCLNT_SHAREMODE_SHARED, 0, requestedDuration, 0, wfx.get(), NULL ); 
	CI_ASSERT( hr == S_OK );

	UINT32 numFrames;
	hr = mCaptureImpl->mAudioClient->GetBufferSize( &numFrames );
	CI_ASSERT( hr == S_OK );

	//double captureDurationMs = (double) numFrames * 1000.0 / (double) wfx->nSamplesPerSec;

	mBlockNumFrames = numFrames; // update capture blocksize with the actual size
	mCaptureImpl->initCapture( numFrames );

	mInterleavedBuffer = BufferInterleaved( numFrames, mNumChannels );
}

void LineInWasapi::uninitialize()
{
	mCaptureImpl->mAudioClient.reset(); // calls Release() on the maintained IAudioClient
}

void LineInWasapi::start()
{
	if( ! mInitialized ) {
		CI_LOG_E( "not initialized" );
		return;
	}

	HRESULT hr = mCaptureImpl->mAudioClient->Start();
	CI_ASSERT( hr == S_OK );

	mEnabled = true;
}

void LineInWasapi::stop()
{
	if( ! mInitialized ) {
		CI_LOG_E( "not initialized" );
		return;
	}

	HRESULT hr = mCaptureImpl->mAudioClient->Stop();
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
// FIXME: RingBuffer read / write returns bool now, update
void LineInWasapi::process( Buffer *buffer )
{
	mCaptureImpl->captureAudio();

	size_t samplesNeeded = buffer->getSize();
	if( mCaptureImpl->mNumSamplesBuffered < samplesNeeded )
		return;

	if( buffer->getNumChannels() == 2 ) {
		size_t numRead = mCaptureImpl->mRingBuffer->read( mInterleavedBuffer.getData(), samplesNeeded );
		dsp::deinterleaveStereoBuffer( &mInterleavedBuffer, buffer );
	} else
		size_t numRead = mCaptureImpl->mRingBuffer->read( buffer->getData(), samplesNeeded );

	mCaptureImpl->mNumSamplesBuffered -= samplesNeeded;
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
