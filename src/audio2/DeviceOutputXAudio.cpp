#include "audio2/DeviceOutputXAudio.h"
#include "audio2/DeviceManagerMsw.h"
#include "audio2/Debug.h"
#include "audio2/assert.h"

#include "cinder/Utilities.h"
#include "cinder/msw/CinderMsw.h"

using namespace std;

namespace audio2 {

DeviceOutputXAudio::DeviceOutputXAudio( const std::string &key )
: Device( key )
{
	LOG_V << "key: " << key << endl;
}

DeviceOutputXAudio::~DeviceOutputXAudio()
{
	LOG_V << "bang" << endl;
	if( mMasteringVoice ) {
		LOG_V << "destroying master voice" << endl;
		mMasteringVoice->DestroyVoice();
	}
	if( mXAudio ) {
		LOG_V << "destroying XAudio" << endl;
		mXAudio->Release();
	}
}

void DeviceOutputXAudio::initialize()
{
#if defined( CINDER_XAUDIO_2_7 )
	LOG_V << "CINDER_XAUDIO_2_7, toolset: v110_xp" << endl;
	UINT32 flags = XAUDIO2_DEBUG_ENGINE;

	ci::msw::initializeCom();

#else
	LOG_V << "CINDER_XAUDIO_2_8, toolset: v110" << endl;
	UINT32 flags = 0;
#endif

	HRESULT hr = ::XAudio2Create( &mXAudio, flags, XAUDIO2_DEFAULT_PROCESSOR );
	CI_ASSERT( hr == S_OK );

#if defined( CINDER_XAUDIO_2_8 )
	::XAUDIO2_DEBUG_CONFIGURATION debugConfig = {0};
	debugConfig.TraceMask = XAUDIO2_LOG_ERRORS;
	debugConfig.BreakMask = XAUDIO2_LOG_ERRORS;
	debugConfig.LogFunctionName = true;
	mXAudio->SetDebugConfiguration( &debugConfig );
#endif

	// mXAudio is started at creation time, so stop it here until after the graph is configured
	stop();

	auto deviceManager = dynamic_cast<DeviceManagerMsw *>( DeviceManager::instance() );
	const wstring &deviceId = deviceManager->getDeviceId( mKey );
	const string &name = deviceManager->getName( mKey );

#if defined( CINDER_XAUDIO_2_8 )
	// TODO: consider moving master voice to Output node
	hr = mXAudio->CreateMasteringVoice( &mMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, deviceId.c_str() );
	CI_ASSERT( hr == S_OK );
#else

	// TODO: on XAudio2.7, mKey (from WASAPI) is the device Id, but this isn't obvious below.  Consider re-mapping getDeviceId() to match this.
	UINT32 deviceCount;
	hr = mXAudio->GetDeviceCount( &deviceCount );
	CI_ASSERT( hr == S_OK );
	::XAUDIO2_DEVICE_DETAILS deviceDetails;
	for( UINT32 i = 0; i < deviceCount; i++ ) {
		hr = mXAudio->GetDeviceDetails( i, &deviceDetails );
		CI_ASSERT( hr == S_OK );
		if( mKey == ci::toUtf8( deviceDetails.DeviceID ) ) {
			LOG_V << "found match: display name: " << deviceDetails.DisplayName << endl;
			LOG_V << "device id: " << deviceDetails.DeviceID << endl;

			hr = mXAudio->CreateMasteringVoice( &mMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, i );
			CI_ASSERT( hr == S_OK );
		}

	}

	CI_ASSERT( mMasteringVoice );

#endif

	::XAUDIO2_VOICE_DETAILS voiceDetails;
	mMasteringVoice->GetVoiceDetails( &voiceDetails );
	LOG_V << "created mastering voice. channels: " << voiceDetails.InputChannels << ", samplerate: " << voiceDetails.InputSampleRate << endl;

}

void DeviceOutputXAudio::uninitialize()
{

}

void DeviceOutputXAudio::start()
{
	HRESULT hr = mXAudio->StartEngine();
	CI_ASSERT( hr ==S_OK );
	LOG_V "started" << endl;
}

void DeviceOutputXAudio::stop()
{
	mXAudio->StopEngine();
	LOG_V "stopped" << endl;
}

} // namespace audio2