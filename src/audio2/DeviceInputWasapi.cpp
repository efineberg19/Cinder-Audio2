#include "audio2/DeviceInputWasapi.h"
#include "audio2/DeviceManagerMsw.h"
#include "audio2/RingBuffer.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"
#include "audio2/msw/util.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>

using namespace std;

namespace audio2 {


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
// MARK: - InputWasapi::Impl
// ----------------------------------------------------------------------------------------------------

struct InputWasapi::Impl {
	::IAudioClient *mAudioClient;
	::IAudioCaptureClient *mAudioCaptureClient;

	Impl() : mAudioClient( nullptr ), mAudioCaptureClient( nullptr ) {}
	~Impl() {
		LOG_V << "BANG" << endl;
		if( mAudioCaptureClient )
			mAudioCaptureClient->Release();
		if( mAudioClient )
			mAudioClient->Release();
	}
};

// ----------------------------------------------------------------------------------------------------
// MARK: - InputWasapi
// ----------------------------------------------------------------------------------------------------

// TODO: samplerate / input channels should be configurable
// TODO: rethink default samplerate / channels
InputWasapi::InputWasapi( DeviceRef device )
: Input( device ), mImpl( new InputWasapi::Impl() )
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

void InputWasapi::initialize()
{
	DeviceManagerMsw *manager = dynamic_cast<DeviceManagerMsw *>( DeviceManagerMsw::instance() );
	CI_ASSERT( manager );

	::IMMDevice *immDevice = manager->getIMMDevice( mDevice->getKey() );

	immDevice->Activate( __uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&mImpl->mAudioClient );

	LOG_E << "complete." << endl;
}

void InputWasapi::uninitialize()
{
	//mDevice->uninitialize();
}

void InputWasapi::start()
{
	//if( ! mDevice->isOutputConnected() ) {
	//	mDevice->start();
	//	LOG_V << "started: " << mDevice->getName() << endl;
	//}
}

void InputWasapi::stop()
{
	//if( ! mDevice->isOutputConnected() ) {
	//	mDevice->stop();
	//	LOG_V << "stopped: " << mDevice->getName() << endl;
	//}
}

DeviceRef InputWasapi::getDevice()
{
	//return std::static_pointer_cast<Device>( mDevice );
	LOG_E << "TODO" << endl;
	return DeviceRef();
}

void InputWasapi::render( BufferT *buffer )
{
	CI_ASSERT( mRingBuffer );

	size_t numFrames = buffer->at( 0 ).size();
	for( size_t c = 0; c < buffer->size(); c++ ) {
		size_t count = mRingBuffer->read( &(*buffer)[c] );
		if( count != numFrames )
			LOG_V << " Warning, unexpected read count: " << count << ", expected: " << numFrames << " (c = " << c << ")" << endl;
	}
}

} // namespace audio2