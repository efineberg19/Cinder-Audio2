#include "audio2/audio.h"
#include "audio2/DeviceManagerCoreAudio.h"
#include "audio2/DeviceAudioUnit.h"
#include "audio2/assert.h"

#include "cinder/cocoa/CinderCocoa.h"

using namespace std;
using namespace ci;

namespace audio2 {

	// some private helpers, not sure yet how widely useful these are
	AudioObjectPropertyAddress audioObjectProperty( AudioObjectPropertySelector propertySelector, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal );
	UInt32 audioObjectPropertyDataSize( ::AudioObjectID objectID, const AudioObjectPropertyAddress& address, UInt32 qualifierDataSize = 0, const void *qualifierData = NULL );
	string audioObjectPropertyString( ::AudioObjectID objectID, AudioObjectPropertySelector propertySelector );
	void audioObjectPropertyData( ::AudioObjectID objectID, const ::AudioObjectPropertyAddress& propertyAddress, UInt32 dataSize, void *data, UInt32 qualifierDataSize = 0, const void *qualifierData = NULL );
	size_t deviceNumChannels( ::AudioDeviceID objectID, bool isInput );
	
// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManagerCoreAudio
// ----------------------------------------------------------------------------------------------------

	DeviceRef DeviceManagerCoreAudio::getDefaultOutput()
	{
		::AudioDeviceID defaultOutputID;
		UInt32 propertySize = sizeof( defaultOutputID );
		::AudioObjectPropertyAddress propertyAddress = audioObjectProperty( kAudioHardwarePropertyDefaultOutputDevice );
		audioObjectPropertyData( kAudioObjectSystemObject, propertyAddress, propertySize, &defaultOutputID );
		return getDevice( DeviceManagerCoreAudio::keyForDeviceID( defaultOutputID ) );
	}

	DeviceRef DeviceManagerCoreAudio::getDefaultInput()
	{
		::AudioDeviceID defaultInputID;
		UInt32 propertySize = sizeof( defaultInputID );
		::AudioObjectPropertyAddress propertyAddress = audioObjectProperty( kAudioHardwarePropertyDefaultInputDevice );
		audioObjectPropertyData( kAudioObjectSystemObject, propertyAddress, propertySize, &defaultInputID );
		return getDevice( DeviceManagerCoreAudio::keyForDeviceID( defaultInputID ) );
	}

	void DeviceManagerCoreAudio::setActiveDevice( const std::string &key )
	{
		AudioDeviceID deviceID = kAudioObjectUnknown;
		shared_ptr<DeviceAudioUnit> deviceAU;
		for( const auto& deviceInfo : getDevices() ) {
			if( deviceInfo.key == key ) {
				deviceAU = dynamic_pointer_cast<DeviceAudioUnit>( deviceInfo.device );
				deviceID = deviceInfo.deviceID;
				break;
			}
		}
		CI_ASSERT( deviceAU );
		CI_ASSERT( deviceAU->mComponentInstance );
		
		OSStatus status = AudioUnitSetProperty( deviceAU->mComponentInstance, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &deviceID, sizeof( deviceID ) );
		CI_ASSERT( status == noErr );
	}

	std::string DeviceManagerCoreAudio::getName( const std::string &key )
	{
		::AudioDeviceID deviceID = getDeviceID( key );
		return audioObjectPropertyString( deviceID, kAudioObjectPropertyName );
	}

	size_t DeviceManagerCoreAudio::getNumInputChannels( const string &key )
	{
		::AudioDeviceID deviceID = getDeviceID( key );
		return deviceNumChannels( deviceID, true );
	}

	size_t DeviceManagerCoreAudio::getNumOutputChannels( const string &key )
	{
		::AudioDeviceID deviceID = getDeviceID( key );
		return deviceNumChannels( deviceID, false );
	}

	size_t DeviceManagerCoreAudio::getSampleRate( const string &key )
	{
		::AudioDeviceID deviceID = getDeviceID( key );
		::AudioObjectPropertyAddress property = audioObjectProperty( kAudioDevicePropertyActualSampleRate );
		Float64 result;
		UInt32 resultSize = sizeof( result );

		audioObjectPropertyData( deviceID, property, resultSize, &result );
		return static_cast<size_t>( result );
	}

	size_t DeviceManagerCoreAudio::getBlockSize( const string &key )
	{
		::AudioDeviceID deviceID = getDeviceID( key );
		::AudioObjectPropertyAddress property = audioObjectProperty( kAudioDevicePropertyBufferFrameSize );
		UInt32 result;
		UInt32 resultSize = sizeof( result );

		audioObjectPropertyData( deviceID, property, resultSize, &result );
		return static_cast<size_t>( result );
	}

// ----------------------------------------------------------------------------------------------------
// MARK: - Private
// ----------------------------------------------------------------------------------------------------

	DeviceRef DeviceManagerCoreAudio::getDevice( const std::string &key )
	{
		for( const auto& deviceInfo : getDevices() ) {
			if( deviceInfo.key == key )
				return deviceInfo.device;
		}
		throw AudioDeviceExc( string( "unknown key: " ) + key );
	}

	::AudioDeviceID DeviceManagerCoreAudio::getDeviceID( const std::string &key )
	{
		for( const auto& deviceInfo : getDevices() ) {
			if( deviceInfo.key == key ) {
				return deviceInfo.deviceID;
			}
		}
		throw AudioDeviceExc( string( "unknown key: " ) + key );
	}

	DeviceManagerCoreAudio::DeviceContainerT& DeviceManagerCoreAudio::getDevices()
	{
		if( mDevices.empty() ) {
			vector<::AudioObjectID> deviceIDs;
			::AudioObjectPropertyAddress devicesProperty = audioObjectProperty( kAudioHardwarePropertyDevices );
			UInt32 devicesPropertySize = audioObjectPropertyDataSize( kAudioObjectSystemObject, devicesProperty );
			size_t numDevices = devicesPropertySize / sizeof( AudioDeviceID );
			deviceIDs.resize( numDevices );

			audioObjectPropertyData( kAudioObjectSystemObject, devicesProperty, devicesPropertySize, deviceIDs.data() );

			::AudioComponentDescription component{ 0 };
			component.componentType = kAudioUnitType_Output;
			component.componentSubType = kAudioUnitSubType_HALOutput;
			component.componentManufacturer = kAudioUnitManufacturer_Apple;

			for ( AudioDeviceID &deviceID : deviceIDs ) {
				string key = keyForDeviceID( deviceID );
				auto device = DeviceRef( new DeviceAudioUnit( component, key ) );
				mDevices.push_back( { key, deviceID, device } );
			}
		}
		return mDevices;
	}

	// note: we cannot just rely on 'model UID', when it is there (which it isn't always), becasue it can be the same
	// for two different 'devices', such as system input and output
	// - current solution: key = 'NAME-[UID | MANUFACTURE]'
	// TODO: use AudioObjectID
	std::string DeviceManagerCoreAudio::keyForDeviceID( ::AudioDeviceID deviceID )
	{
		string name = audioObjectPropertyString( deviceID, kAudioObjectPropertyName );
		string key = audioObjectPropertyString( deviceID, kAudioDevicePropertyModelUID );
		if( key.empty() )
			key = audioObjectPropertyString( deviceID, kAudioObjectPropertyManufacturer );

		return name + " - " + key;
	}

// ----------------------------------------------------------------------------------------------------
// MARK: - Core Audio Helpers
// ----------------------------------------------------------------------------------------------------


	AudioObjectPropertyAddress audioObjectProperty( ::AudioObjectPropertySelector propertySelector, ::AudioObjectPropertyScope scope )
	{
		::AudioObjectPropertyAddress result;
		result.mSelector = propertySelector;
		result.mScope = scope;
		result.mElement = kAudioObjectPropertyElementMaster;
		return result;
	}

	UInt32 audioObjectPropertyDataSize( ::AudioObjectID objectID, const ::AudioObjectPropertyAddress& propertyAddress, UInt32 qualifierDataSize, const void *qualifierData )
	{
		UInt32 result = 0;
		OSStatus status = ::AudioObjectGetPropertyDataSize( objectID, &propertyAddress, qualifierDataSize, qualifierData, &result );
		CI_ASSERT( status == noErr );

		return result;
	}

	void audioObjectPropertyData( ::AudioObjectID objectID, const ::AudioObjectPropertyAddress& propertyAddress, UInt32 dataSize, void *data, UInt32 qualifierDataSize, const void* qualifierData )
	{
		OSStatus status = ::AudioObjectGetPropertyData( objectID, &propertyAddress, qualifierDataSize, qualifierData, &dataSize, data );
		CI_ASSERT( status == noErr );
	}

	string audioObjectPropertyString( ::AudioObjectID objectID, ::AudioObjectPropertySelector propertySelector )
	{
		::AudioObjectPropertyAddress property = audioObjectProperty( propertySelector );
		if( !::AudioObjectHasProperty( objectID, &property ) )
			return string();

		CFStringRef resultCF;
		UInt32 cfStringSize = sizeof( CFStringRef );

		OSStatus status = ::AudioObjectGetPropertyData( objectID, &property, 0, NULL, &cfStringSize, &resultCF );
		CI_ASSERT( status == noErr );

		string result = cocoa::convertCfString( resultCF );
		CFRelease( resultCF );
		return result;
	}

	size_t deviceNumChannels( ::AudioObjectID objectID, bool isInput )
	{
		::AudioObjectPropertyAddress streamConfigProperty = audioObjectProperty( kAudioDevicePropertyStreamConfiguration, isInput ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput );
		UInt32 streamConfigPropertySize = audioObjectPropertyDataSize( objectID, streamConfigProperty );
		shared_ptr<::AudioBufferList> bufferList( (::AudioBufferList *)calloc( 1, streamConfigPropertySize ), free );

		audioObjectPropertyData( objectID, streamConfigProperty, streamConfigPropertySize,  bufferList.get() );

		size_t numChannels = 0;
		for( int i = 0; i < bufferList->mNumberBuffers; i++ ) {
			numChannels += bufferList->mBuffers[i].mNumberChannels;
		}
		return numChannels;
	}


} // namespace audio2