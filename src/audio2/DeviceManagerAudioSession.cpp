#include "audio2/DeviceManagerAudioSession.h"
#include "audio2/DeviceAudioUnit.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

#include <cmath>

#include <AudioToolbox/AudioToolbox.h>

using namespace std;

// TODO: AudioSession API supports routes, such as kAudioSessionOutputRoute_AirPlay
// - these could surely be exposed
// - map them to separate devices?

namespace audio2 {

	const string kRemoteIOKey = "iOS-RemoteIO";

	template <typename ResultT>
	inline void audioSessionProperty( ::AudioSessionPropertyID property, ResultT &result )
	{
		UInt32 resultSize = sizeof( result );
		OSStatus status = ::AudioSessionGetProperty( property, &resultSize, &result );
		CI_ASSERT( status == noErr );
	}

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManagerAudioSession
// ----------------------------------------------------------------------------------------------------

DeviceManagerAudioSession::DeviceManagerAudioSession()
: DeviceManager()
{
	// TODO: install interrupt listener
	OSStatus status = AudioSessionInitialize( NULL, NULL, NULL, NULL );
	CI_ASSERT( status == noErr );
}

DeviceRef DeviceManagerAudioSession::getDefaultOutput()
{
	return getRemoteIOUnit();
}

DeviceRef DeviceManagerAudioSession::getDefaultInput()
{
	return getRemoteIOUnit();
}

void DeviceManagerAudioSession::setActiveDevice( const std::string &key )
{
	// ???: need to call AudioSessionSetActive? current cinder cocoa touch impl does not, while libpd does
}

std::string DeviceManagerAudioSession::getName( const std::string &key )
{
	return kRemoteIOKey;
}

size_t DeviceManagerAudioSession::getNumInputChannels( const string &key )
{
	UInt32 result;
	audioSessionProperty( kAudioSessionProperty_CurrentHardwareInputNumberChannels, result );
	return static_cast<size_t>( result );
}

size_t DeviceManagerAudioSession::getNumOutputChannels( const string &key )
{
	UInt32 result;
	audioSessionProperty( kAudioSessionProperty_CurrentHardwareOutputNumberChannels, result );
	return static_cast<size_t>( result );
}

size_t DeviceManagerAudioSession::getSampleRate( const string &key )
{
	Float64 result;
	audioSessionProperty( kAudioSessionProperty_CurrentHardwareSampleRate, result );
	return static_cast<size_t>( result );
}

size_t DeviceManagerAudioSession::getBlockSize( const string &key )
{
	Float32 durationSeconds;
	audioSessionProperty( kAudioSessionProperty_CurrentHardwareIOBufferDuration, durationSeconds );
	return std::lround( static_cast<Float32>( getSampleRate( key ) ) * durationSeconds );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - Private
// ----------------------------------------------------------------------------------------------------

DeviceRef DeviceManagerAudioSession::getRemoteIOUnit()
{
	if( ! mRemoteIOUnit ) {
		::AudioComponentDescription component{ 0 };
		component.componentType = kAudioUnitType_Output;
		component.componentSubType = kAudioUnitSubType_RemoteIO;
		component.componentManufacturer = kAudioUnitManufacturer_Apple;

		mRemoteIOUnit = DeviceRef( new DeviceAudioUnit( component, kRemoteIOKey ) );
	}

	return mRemoteIOUnit;
}

	
} // namespace audio2