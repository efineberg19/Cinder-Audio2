#include "audio2/DeviceInputWasapi.h"
#include "audio2/DeviceManagerMsw.h"
#include "audio2/audio.h"
#include "audio2/RingBuffer.h"
#include "audio2/UGen.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"
#include "audio2/msw/util.h"

#include "cinder/Thread.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>

using namespace std;

namespace audio2 {

// converts from from milliseconds to 100-nanoseconds
static inline ::REFERENCE_TIME toReferenceTime( size_t ms ) {
	return ms * 10000;
}

struct InputWasapi::Impl {
	Impl() : mCaptureInitialized( false ) {}
	~Impl() {}

	void initCapture( size_t bufferSize );
	void captureAudio();

	std::unique_ptr<::IAudioClient, msw::ComReleaser>			mAudioClient;
	std::unique_ptr<::IAudioCaptureClient, msw::ComReleaser>	mCaptureClient;
	std::unique_ptr<std::thread>	mCaptureThread;
	std::unique_ptr<RingBuffer>		mRingBuffer;
	::HANDLE						mCaptureEvent;
	atomic<bool> mCaptureInitialized;
};

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

// TODO: samplerate / input channels should be configurable
// TODO: rethink default samplerate / channels
InputWasapi::InputWasapi( DeviceRef device )
: Input( device ), mImpl( new InputWasapi::Impl() ), mCaptureDurationMs( 20 )
{
	mTag = "InputWasapi";

	mDevice = dynamic_pointer_cast<DeviceInputWasapi>( device );
	CI_ASSERT( mDevice );

	mFormat.setSampleRate( mDevice->getSampleRate() );
	mFormat.setNumChannels( 2 );
}

InputWasapi::~InputWasapi()
{
}

// TODO: Most of this, up to format setting, can be in constructor
// - this simplifies re-initialization of input
void InputWasapi::initialize()
{
	DeviceManagerMsw *manager = dynamic_cast<DeviceManagerMsw *>( DeviceManagerMsw::instance() );
	CI_ASSERT( manager );

	shared_ptr<::IMMDevice> immDevice = manager->getIMMDevice( mDevice->getKey() );

	::IAudioClient *audioClient;
	HRESULT hr = immDevice->Activate( __uuidof(::IAudioClient), CLSCTX_ALL, NULL, (void**)&audioClient );
	CI_ASSERT( hr == S_OK );
	mImpl->mAudioClient = msw::makeComUnique( audioClient );

	::WAVEFORMATEX *mixFormat;
	hr = mImpl->mAudioClient->GetMixFormat( &mixFormat );
	CI_ASSERT( hr == S_OK );

	LOG_V << "initial mix format samplerate: " << mixFormat->nSamplesPerSec << ", num channels: " << mixFormat->nChannels << endl;
	CoTaskMemFree( mixFormat );

	auto wfx = msw::interleavedFloatWaveFormat( mFormat.getNumChannels(), mFormat.getSampleRate() );
	::WAVEFORMATEX *closestMatch;
	hr = mImpl->mAudioClient->IsFormatSupported( ::AUDCLNT_SHAREMODE_SHARED, wfx.get(), &closestMatch );
	if( hr == S_OK )
		LOG_V << "requested format is supported." << endl;
	else if( hr == S_FALSE ) {
		CI_ASSERT( closestMatch );
		LOG_V << "cannot use requested format. TODO: use closest" << endl;
	}
	else
		throw AudioFormatExc( "Could not find a suitable format for IAudioCaptureClient" );

	LOG_V << "requested duration: " << mCaptureDurationMs << "ms" << endl;

	::REFERENCE_TIME requestedDuration = toReferenceTime( mCaptureDurationMs );
	DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
	hr = mImpl->mAudioClient->Initialize( ::AUDCLNT_SHAREMODE_SHARED, streamFlags, requestedDuration, 0, wfx.get(), NULL ); 
	CI_ASSERT( hr == S_OK );

	UINT32 bufferSize;
	hr = mImpl->mAudioClient->GetBufferSize( &bufferSize );
	CI_ASSERT( hr == S_OK );

	mCaptureDurationMs = (size_t)( (double) bufferSize * 1000.0 / (double) wfx->nSamplesPerSec );

	LOG_V << "bufferSize: " << bufferSize << ", actual duration: " << mCaptureDurationMs << "ms" << endl;

	mImpl->initCapture( bufferSize );

	mInterleavedBuffer.resize( bufferSize );
	
	mInitialized = true;
	LOG_V << "complete." << endl;
}

// TODO: consider uninitializing device
void InputWasapi::uninitialize()
{
	if( ! mInitialized )
		return;

	HRESULT hr = mImpl->mAudioClient->Reset();
	CI_ASSERT( hr == S_OK );

	mInitialized = false;
}

// ???: is there a way to re-use the thread so it doesn't need to be allocated each start / stop?
// - could probably create thread in init and just wait. then start the client here
void InputWasapi::start()
{
	if( ! mInitialized ) {
		LOG_E << "not initialized" << endl;
		return;
	}
	
	HRESULT hr = mImpl->mAudioClient->Start();
	CI_ASSERT( hr == S_OK );
}

void InputWasapi::stop()
{
	if( ! mInitialized ) {
		LOG_E << "not initialized" << endl;
		return;
	}

	HRESULT hr = mImpl->mAudioClient->Stop();
	CI_ASSERT( hr == S_OK );
}

DeviceRef InputWasapi::getDevice()
{
	return std::static_pointer_cast<Device>( mDevice );
}

// FIXME: This doesn't handle mismatched buffer sizes well at all
// - default capture size is currently too small anyway
void InputWasapi::render( BufferT *buffer )
{
	size_t samplesNeeded = buffer->size() * buffer->at( 0 ).size();
	mImpl->mRingBuffer->read( mInterleavedBuffer.data(), samplesNeeded );

	deinterleaveStereoBuffer( &mInterleavedBuffer, buffer );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - InputWasapi::Impl
// ----------------------------------------------------------------------------------------------------

void InputWasapi::Impl::initCapture( size_t bufferSize ) {
	CI_ASSERT( mAudioClient );

	mCaptureEvent = ::CreateEventEx( nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE );
	CI_ASSERT( mCaptureEvent );

	HRESULT hr = mAudioClient->SetEventHandle( mCaptureEvent );
	CI_ASSERT( hr == S_OK );

	::IAudioCaptureClient *captureClient;
	hr = mAudioClient->GetService( __uuidof(IAudioCaptureClient), (void**)&captureClient );
	CI_ASSERT( hr == S_OK );
	mCaptureClient = msw::makeComUnique( captureClient );

	mRingBuffer.reset( new RingBuffer( bufferSize ) );

	mCaptureInitialized = true;
	mCaptureThread = unique_ptr<thread>( new thread( bind( &InputWasapi::Impl::captureAudio, this ) ) );
}

void InputWasapi::Impl::captureAudio() {
	while( mCaptureInitialized ) {
		DWORD waitResult = ::WaitForSingleObject( mCaptureEvent, INFINITE );

		BYTE *audioData;
		UINT32 numFramesAvailable;
		DWORD flags;
		HRESULT hr = mCaptureClient->GetBuffer( &audioData, &numFramesAvailable, &flags, NULL, NULL );
		switch( hr ) {
		case S_OK: break;
		case AUDCLNT_S_BUFFER_EMPTY: LOG_V << "AUDCLNT_S_BUFFER_EMPTY" << endl; continue;
		case AUDCLNT_E_BUFFER_ERROR: LOG_V << "AUDCLNT_E_BUFFER_ERROR" << endl; return;
		case AUDCLNT_E_OUT_OF_ORDER: LOG_V << "AUDCLNT_E_OUT_OF_ORDER" << endl; return;
		case AUDCLNT_E_DEVICE_INVALIDATED: LOG_V << "AUDCLNT_E_DEVICE_INVALIDATED" << endl; return;
		case AUDCLNT_E_BUFFER_OPERATION_PENDING: LOG_V << "AUDCLNT_E_BUFFER_OPERATION_PENDING" << endl; return;
		case AUDCLNT_E_SERVICE_NOT_RUNNING: LOG_V << "AUDCLNT_E_SERVICE_NOT_RUNNING" << endl; return;
		case E_POINTER: LOG_V << "E_POINTER" << endl; return;
		default: LOG_V << "unknown" << endl; return;
		}

		if ( flags & AUDCLNT_BUFFERFLAGS_SILENT ) {
			LOG_V << "silence. TODO: fil buffer with zeros." << endl;
			// ???: ignore this? copying the samples is just about the same as setting to 0
			//fill( mCaptureBuffer.begin(), mCaptureBuffer.end(), 0.0f );
		}
		else {
			float *samples = (float *)audioData;

			// TODO: make sure this is doing the right thing when there is:
			//	- A) more samples than the buffer can hold
			//  - B) not enough samples to fill the entire block size
			mRingBuffer->write( samples, numFramesAvailable );
		}

		hr = mCaptureClient->ReleaseBuffer( numFramesAvailable );
		CI_ASSERT( hr == S_OK );
	}
}

} // namespace audio2