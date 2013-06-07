#include "audio2/DeviceInputWasapi.h"
#include "audio2/DeviceManagerMsw.h"
#include "audio2/audio.h"
#include "audio2/RingBuffer.h"
#include "audio2/Dsp.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"
#include "audio2/msw/util.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>

using namespace std;

namespace audio2 {

struct InputWasapi::Impl {
	Impl() : mNumSamplesBuffered( 0 ) {}
	~Impl() {}

	void initCapture( size_t numFrames, size_t numChannels );
	void captureAudio();

	std::unique_ptr<::IAudioClient, msw::ComReleaser>			mAudioClient;
	std::unique_ptr<::IAudioCaptureClient, msw::ComReleaser>	mCaptureClient;
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
//  - first set in Graph?
//  - nodes can override in format
InputWasapi::InputWasapi( DeviceRef device )
: InputNode( device ), mImpl( new InputWasapi::Impl() ), mCaptureBlockSize( 1024 )
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
	hr = mImpl->mAudioClient->Initialize( ::AUDCLNT_SHAREMODE_SHARED, 0, requestedDuration, 0, wfx.get(), NULL ); 
	CI_ASSERT( hr == S_OK );

	UINT32 numFrames;
	hr = mImpl->mAudioClient->GetBufferSize( &numFrames );
	CI_ASSERT( hr == S_OK );

	double captureDurationMs = (double) numFrames * 1000.0 / (double) wfx->nSamplesPerSec;

	mCaptureBlockSize = numFrames;
	mImpl->initCapture( numFrames, mFormat.getNumChannels() );

	mInterleavedBuffer = Buffer( mFormat.getNumChannels(), numFrames, Buffer::Format::Interleaved );
	
	LOG_V << "numFrames: " << numFrames << ", buffer size: " << mInterleavedBuffer.getSize() << ", actual duration: " << captureDurationMs << "ms" << endl;

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

// TODO: decide what to do when there is a buffer under/over run. LOG_V / app::console() is not thread-safe..
void InputWasapi::render( Buffer *buffer )
{
	mImpl->captureAudio();

	size_t samplesNeeded = buffer->getSize();
	if( mImpl->mNumSamplesBuffered < samplesNeeded ) {
		//LOG_V << "BUFFER UNDERRUN. needed: " << samplesNeeded << ", available: " << mImpl->mNumSamplesBuffered << endl;
		return;
	}

	if( buffer->getNumChannels() == 2 ) {
		size_t numRead = mImpl->mRingBuffer->read( mInterleavedBuffer.getData(), samplesNeeded );
		CI_ASSERT( numRead == samplesNeeded );

		deinterleaveStereoBuffer( &mInterleavedBuffer, buffer );
	} else {
		size_t numRead = mImpl->mRingBuffer->read( buffer->getData(), samplesNeeded );
		CI_ASSERT( numRead == samplesNeeded );
	}

	mImpl->mNumSamplesBuffered -= samplesNeeded;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - InputWasapi::Impl
// ----------------------------------------------------------------------------------------------------

void InputWasapi::Impl::initCapture( size_t numFrames, size_t numChannels ) {
	CI_ASSERT( mAudioClient );

	mNumChannels = numChannels;

	::IAudioCaptureClient *captureClient;
	HRESULT hr = mAudioClient->GetService( __uuidof(::IAudioCaptureClient), (void**)&captureClient );
	CI_ASSERT( hr == S_OK );
	mCaptureClient = msw::makeComUnique( captureClient );

	mRingBuffer.reset( new RingBuffer( numFrames * numChannels ) );
}

void InputWasapi::Impl::captureAudio()
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

} // namespace audio2