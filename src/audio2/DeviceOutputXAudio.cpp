#include "audio2/DeviceOutputXAudio.h"
#include "audio2/DeviceManagerMsw.h"
#include "audio2/Debug.h"
#include "audio2/assert.h"

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

#else
	LOG_V << "CINDER_XAUDIO_2_8, toolset: v110" << endl;
	UINT32 flags = 0;
#endif

	HRESULT hr = ::XAudio2Create( &mXAudio, flags, XAUDIO2_DEFAULT_PROCESSOR );
	CI_ASSERT( hr == S_OK );

#if defined( CINDER_XAUDIO_2_8 )
	::XAUDIO2_DEBUG_CONFIGURATION debugConfig = {0};
	debugConfig.TraceMask = XAUDIO2_LOG_DETAIL;
	debugConfig.BreakMask = XAUDIO2_LOG_WARNINGS;
	debugConfig.LogFunctionName = true;
	mXAudio->SetDebugConfiguration( &debugConfig );
#endif

	// mXAudio is started at creation time, so stop it here until after the graph is configured
	stop();

#if defined( CINDER_XAUDIO_2_8 )
	// TODO: consider moving master voice to Output node
	const wstring &deviceId = dynamic_cast<DeviceManagerMsw *>( DeviceManager::instance() )->getDeviceId( mKey );
	hr = mXAudio->CreateMasteringVoice( &mMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, deviceId.c_str() );
	CI_ASSERT( hr == S_OK );

	// parse voice details

	::XAUDIO2_VOICE_DETAILS voiceDetails;
	mMasteringVoice->GetVoiceDetails( &voiceDetails );
	//mNumChannels = voiceDetails.InputChannels;
	//mSampleRate = voiceDetails.InputSampleRate;

#else
	// TODO: pick device with index
	HRESULT hr = device->mXAudio->CreateMasteringVoice( &device->mMasteringVoice );
	CI_ASSERT( ! FAILED( hr ) );

	CI_ASSERT( 0 ); // TODO: fill out params

	//UINT32 deviceCount;
	//XAUDIO2_DEVICE_DETAILS deviceDetails;
	//device->mXAudio->GetDeviceCount( &deviceCount );
	//for( UINT32 i = 0; i < deviceCount; i++ ) {
	//	device->mXAudio->GetDeviceDetails( 0, &deviceDetails );
	//	if( deviceDetails.Role == GlobalDefaultDevice ) {
	//		LOG_V << "default master voice created." << endl;
	//		break;
	//	}
	//}

#endif

	LOG_V << "init complete" << endl;
}

void DeviceOutputXAudio::uninitialize()
{

}

void DeviceOutputXAudio::start()
{
	LOG_V "starting" << endl;
	HRESULT hr = mXAudio->StartEngine();
	CI_ASSERT( hr ==S_OK );
}

void DeviceOutputXAudio::stop()
{
	LOG_V "stopping" << endl;
	mXAudio->StopEngine();
}

} // namespace audio2