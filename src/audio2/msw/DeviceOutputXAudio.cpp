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

#include "audio2/msw/DeviceOutputXAudio.h"
#include "audio2/msw/DeviceManagerWasapi.h"
#include "audio2/Debug.h"
#include "audio2/CinderAssert.h"

#include "cinder/Utilities.h"
#include "cinder/msw/CinderMsw.h"

using namespace std;

namespace cinder { namespace audio2 { namespace msw {

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

	auto deviceManager = dynamic_cast<DeviceManagerWasapi *>( DeviceManager::instance() );
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

} } } // namespace cinder::audio2::msw