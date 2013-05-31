#include "audio2/DeviceInputWasapi.h"
#include "audio2/DeviceManagerMsw.h"
#include "audio2/audio.h"
#include "audio2/RingBuffer.h"
#include "audio2/Dsp.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"
#include "audio2/msw/util.h"

#include "cinder/Thread.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>

using namespace std;

namespace audio2 {

struct InputWasapi::Impl {
	Impl() : mCaptureInitialized( false ) {}
	~Impl();

	void initCapture( size_t bufferSize );
	void captureAudio();

	std::unique_ptr<::IAudioClient, msw::ComReleaser>			mAudioClient;
	std::unique_ptr<::IAudioCaptureClient, msw::ComReleaser>	mCaptureClient;
	std::unique_ptr<std::thread>	mCaptureThread;
	std::unique_ptr<RingBuffer>		mRingBuffer;
	::HANDLE						mCaptureEvent;
	atomic<bool> mCaptureInitialized;
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
//  - first set in Graph?
//  - nodes can override in format
InputWasapi::InputWasapi( DeviceRef device )
: Input( device ), mImpl( new InputWasapi::Impl() ), mCaptureBlockSize( 512 )
{
	mTag = "InputWasapi";

	mDevice = dynamic_pointer_cast<DeviceInputWasapi>( device );
	CI_ASSERT( mDevice );

	

	DeviceManagerMsw *manager = dynamic_cast<DeviceManagerMsw *>( DeviceManagerMsw::instance() );
	CI_ASSERT( manager );

	shared_ptr<::IMMDevice> immDevice = manager->getIMMDevice( mDevice->getKey() );

	::IAudioClient *audioClient;
	HRESULT hr = immDevice->Activate( __uuidof(::IAudioClient), CLSCTX_ALL, NULL, (void**)&audioClient );
	CI_ASSERT( hr == S_OK );
	mImpl->mAudioClient = msw::makeComUnique( audioClient );

	// set default format to match the audio client's defaults

	::WAVEFORMATEX *mixFormat;
	hr = mImpl->mAudioClient->GetMixFormat( &mixFormat );
	CI_ASSERT( hr == S_OK );

	mFormat.setSampleRate( mixFormat->nSamplesPerSec );
	mFormat.setNumChannels( mixFormat->nChannels );

	LOG_V << "initial mix format samplerate: " << mixFormat->nSamplesPerSec << ", num channels: " << mixFormat->nChannels << endl;
	CoTaskMemFree( mixFormat );

	LOG_V << "activated audio client" << endl;
}

InputWasapi::~InputWasapi()
{
}

// TODO: properly handle channel counts != 2
void InputWasapi::initialize()
{
	auto wfx = msw::interleavedFloatWaveFormat( mFormat.getNumChannels(), mFormat.getSampleRate() );
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

	::REFERENCE_TIME requestedDuration = samplesToReferenceTime( mCaptureBlockSize, mFormat.getSampleRate() );
	DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
	hr = mImpl->mAudioClient->Initialize( ::AUDCLNT_SHAREMODE_SHARED, streamFlags, requestedDuration, 0, wfx.get(), NULL ); 
	CI_ASSERT( hr == S_OK );

	UINT32 numFrames;
	hr = mImpl->mAudioClient->GetBufferSize( &numFrames );
	CI_ASSERT( hr == S_OK );

	double captureDurationMs = (double) numFrames * 1000.0 / (double) wfx->nSamplesPerSec;
	LOG_V << "numFrames: " << numFrames << ", actual duration: " << captureDurationMs << "ms" << endl;

	mCaptureBlockSize = numFrames;
	size_t bufferSize = numFrames * mFormat.getNumChannels();
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

	mImpl->mCaptureInitialized = false;
	HRESULT hr = mImpl->mAudioClient->Reset();
	CI_ASSERT( hr == S_OK );

	mInitialized = false;
}

void InputWasapi::start()
{
	if( ! mInitialized ) {
		LOG_E << "not initialized" << endl;
		return;
	}
	
	HRESULT hr = mImpl->mAudioClient->Start();
	CI_ASSERT( hr == S_OK );

	LOG_V << "started " << mDevice->getName() << endl;
}

// FIXME: stop is not blocking the capture loop as I expected...  segfaults are occuring.
void InputWasapi::stop()
{
	if( ! mInitialized ) {
		LOG_E << "not initialized" << endl;
		return;
	}

	HRESULT hr = mImpl->mAudioClient->Stop();
	CI_ASSERT( hr == S_OK );

	LOG_V << "stopped " << mDevice->getName() << endl;
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

InputWasapi::Impl::~Impl()
{
	mCaptureThread->detach();
}

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

// TODO: try fetching samples from XAudio's thread instead of creating our own
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