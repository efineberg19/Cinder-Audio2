#include "audio2/DeviceManagerAudioSession.h"
#include "audio2/DeviceAudioUnit.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

#include <cmath>

#include <AudioToolbox/AudioToolbox.h>

using namespace std;

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
: DeviceManager(), mSessionIsActive( false )
{
	// TODO: install interrupt listener
	OSStatus status = ::AudioSessionInitialize( NULL, NULL, NULL, NULL );
	CI_ASSERT( status == noErr );
}

DeviceRef DeviceManagerAudioSession::getDefaultOutput()
{
	return static_pointer_cast<Device>( getRemoteIOUnit() );
}

DeviceRef DeviceManagerAudioSession::getDefaultInput()
{
	return static_pointer_cast<Device>( getRemoteIOUnit() );
}

void DeviceManagerAudioSession::setActiveDevice( const std::string &key )
{
	LOG_V << "bang" << endl;

	activateSession();

	auto device = dynamic_pointer_cast<DeviceAudioUnit>( getRemoteIOUnit() );
	if( device->isInputConnected() ) {

		LOG_V << "setting category to kAudioSessionCategory_PlayAndRecord" << endl;
		UInt32 category = kAudioSessionCategory_PlayAndRecord;
		OSStatus status = ::AudioSessionSetProperty( kAudioSessionProperty_AudioCategory, sizeof( category ), &category );
		CI_ASSERT( status == noErr );
	}
}

// TODO: check input available property as well
// TODO: this reports 0 channels with default category. To overcome this, can switch to playAndRecord, check, and then return to previous
bool DeviceManagerAudioSession::inputIsEnabled()
{
	UInt32 category = getSessionCategory();
	return ( category == kAudioSessionCategory_PlayAndRecord || category == kAudioSessionCategory_RecordAudio );
}

std::string DeviceManagerAudioSession::getName( const std::string &key )
{
	return kRemoteIOKey;
}

size_t DeviceManagerAudioSession::getNumInputChannels( const string &key )
{
	if( ! inputIsEnabled() ) {
//		LOG_V << "Warning: input is disabled due to session category, so no inputs." << endl;
		return 0;
	}
	
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

shared_ptr<DeviceAudioUnit> DeviceManagerAudioSession::getRemoteIOUnit()
{
	if( ! mRemoteIOUnit ) {
		::AudioComponentDescription component{ 0 };
		component.componentType = kAudioUnitType_Output;
		component.componentSubType = kAudioUnitSubType_RemoteIO;
		component.componentManufacturer = kAudioUnitManufacturer_Apple;

		mRemoteIOUnit = shared_ptr<DeviceAudioUnit>( new DeviceAudioUnit( component, kRemoteIOKey ) );
	}

	return mRemoteIOUnit;
}

void DeviceManagerAudioSession::activateSession()
{
	OSStatus status = ::AudioSessionSetActive( true );
	CI_ASSERT( status == noErr );

	mSessionIsActive = true;
}

UInt32	DeviceManagerAudioSession::getSessionCategory()
{
	UInt32 result;
	audioSessionProperty( kAudioSessionProperty_AudioCategory, result );
	return result;
}

} // namespace audio2