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
UInt32 audioObjectPropertyDataSize( ::AudioObjectID objectId, const AudioObjectPropertyAddress& address, UInt32 qualifierDataSize = 0, const void *qualifierData = NULL );
string audioObjectPropertyString( ::AudioObjectID objectId, AudioObjectPropertySelector propertySelector );
void audioObjectPropertyData( ::AudioObjectID objectId, const ::AudioObjectPropertyAddress& propertyAddress, UInt32 dataSize, void *data, UInt32 qualifierDataSize = 0, const void *qualifierData = NULL );
size_t deviceNumChannels( ::AudioDeviceID objectId, bool isInput );

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManagerCoreAudio
// ----------------------------------------------------------------------------------------------------

DeviceRef DeviceManagerCoreAudio::getDefaultOutput()
{
	::AudioDeviceID defaultOutputId;
	UInt32 propertySize = sizeof( defaultOutputId );
	::AudioObjectPropertyAddress propertyAddress = audioObjectProperty( kAudioHardwarePropertyDefaultOutputDevice );
	audioObjectPropertyData( kAudioObjectSystemObject, propertyAddress, propertySize, &defaultOutputId );
	return getDevice( DeviceManagerCoreAudio::keyForDeviceId( defaultOutputId ) );
}

DeviceRef DeviceManagerCoreAudio::getDefaultInput()
{
	::AudioDeviceID defaultInputId;
	UInt32 propertySize = sizeof( defaultInputId );
	::AudioObjectPropertyAddress propertyAddress = audioObjectProperty( kAudioHardwarePropertyDefaultInputDevice );
	audioObjectPropertyData( kAudioObjectSystemObject, propertyAddress, propertySize, &defaultInputId );
	return getDevice( DeviceManagerCoreAudio::keyForDeviceId( defaultInputId ) );
}

void DeviceManagerCoreAudio::setActiveDevice( const string &key )
{
	::AudioDeviceID deviceId = kAudioObjectUnknown;
	shared_ptr<DeviceAudioUnit> deviceAU;
	for( const auto& deviceInfo : getDevices() ) {
		if( deviceInfo.key == key ) {
			deviceAU = dynamic_pointer_cast<DeviceAudioUnit>( deviceInfo.device );
			deviceId = deviceInfo.deviceId;
			break;
		}
	}
	CI_ASSERT( deviceAU );

	OSStatus status = ::AudioUnitSetProperty( deviceAU->getComponentInstance(), kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &deviceId, sizeof( deviceId ) );
	CI_ASSERT( status == noErr );
}

std::string DeviceManagerCoreAudio::getName( const string &key )
{
	::AudioDeviceID deviceId = getDeviceId( key );
	return audioObjectPropertyString( deviceId, kAudioObjectPropertyName );
}

size_t DeviceManagerCoreAudio::getNumInputChannels( const string &key )
{
	::AudioDeviceID deviceId = getDeviceId( key );
	return deviceNumChannels( deviceId, true );
}

size_t DeviceManagerCoreAudio::getNumOutputChannels( const string &key )
{
	::AudioDeviceID deviceId = getDeviceId( key );
	return deviceNumChannels( deviceId, false );
}

size_t DeviceManagerCoreAudio::getSampleRate( const string &key )
{
	::AudioDeviceID deviceId = getDeviceId( key );
	::AudioObjectPropertyAddress property = audioObjectProperty( kAudioDevicePropertyActualSampleRate );
	Float64 result;
	UInt32 resultSize = sizeof( result );

	audioObjectPropertyData( deviceId, property, resultSize, &result );
	return static_cast<size_t>( result );
}

size_t DeviceManagerCoreAudio::getBlockSize( const string &key )
{
	::AudioDeviceID deviceId = getDeviceId( key );
	::AudioObjectPropertyAddress property = audioObjectProperty( kAudioDevicePropertyBufferFrameSize );
	UInt32 result;
	UInt32 resultSize = sizeof( result );

	audioObjectPropertyData( deviceId, property, resultSize, &result );
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

::AudioDeviceID DeviceManagerCoreAudio::getDeviceId( const std::string &key )
{
	for( const auto& deviceInfo : getDevices() ) {
		if( deviceInfo.key == key ) {
			return deviceInfo.deviceId;
		}
	}
	throw AudioDeviceExc( string( "unknown key: " ) + key );
}

DeviceManagerCoreAudio::DeviceContainerT& DeviceManagerCoreAudio::getDevices()
{
	if( mDevices.empty() ) {
		vector<::AudioObjectID> deviceIds;
		::AudioObjectPropertyAddress devicesProperty = audioObjectProperty( kAudioHardwarePropertyDevices );
		UInt32 devicesPropertySize = audioObjectPropertyDataSize( kAudioObjectSystemObject, devicesProperty );
		size_t numDevices = devicesPropertySize / sizeof( ::AudioDeviceID );
		deviceIds.resize( numDevices );

		audioObjectPropertyData( kAudioObjectSystemObject, devicesProperty, devicesPropertySize, deviceIds.data() );

		::AudioComponentDescription component{ 0 };
		component.componentType = kAudioUnitType_Output;
		component.componentSubType = kAudioUnitSubType_HALOutput;
		component.componentManufacturer = kAudioUnitManufacturer_Apple;

		for ( ::AudioDeviceID &deviceId : deviceIds ) {
			string key = keyForDeviceId( deviceId );
			auto device = DeviceRef( new DeviceAudioUnit( component, key ) );
			mDevices.push_back( { key, deviceId, device } );
		}
	}
	return mDevices;
}

// note: we cannot just rely on 'model UID', when it is there (which it isn't always), becasue it can be the same
// for two different 'devices', such as system input and output
// - current solution: key = 'NAME-[UID | MANUFACTURE]'
std::string DeviceManagerCoreAudio::keyForDeviceId( ::AudioObjectID deviceId )
{
	string name = audioObjectPropertyString( deviceId, kAudioObjectPropertyName );
	string key = audioObjectPropertyString( deviceId, kAudioDevicePropertyModelUID );
	if( key.empty() )
		key = audioObjectPropertyString( deviceId, kAudioObjectPropertyManufacturer );

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

UInt32 audioObjectPropertyDataSize( ::AudioObjectID objectId, const ::AudioObjectPropertyAddress& propertyAddress, UInt32 qualifierDataSize, const void *qualifierData )
{
	UInt32 result = 0;
	OSStatus status = ::AudioObjectGetPropertyDataSize( objectId, &propertyAddress, qualifierDataSize, qualifierData, &result );
	CI_ASSERT( status == noErr );

	return result;
}

void audioObjectPropertyData( ::AudioObjectID objectId, const ::AudioObjectPropertyAddress& propertyAddress, UInt32 dataSize, void *data, UInt32 qualifierDataSize, const void* qualifierData )
{
	OSStatus status = ::AudioObjectGetPropertyData( objectId, &propertyAddress, qualifierDataSize, qualifierData, &dataSize, data );
	CI_ASSERT( status == noErr );
}

string audioObjectPropertyString( ::AudioObjectID objectId, ::AudioObjectPropertySelector propertySelector )
{
	::AudioObjectPropertyAddress property = audioObjectProperty( propertySelector );
	if( !::AudioObjectHasProperty( objectId, &property ) )
		return string();

	CFStringRef resultCF;
	UInt32 cfStringSize = sizeof( CFStringRef );

	OSStatus status = ::AudioObjectGetPropertyData( objectId, &property, 0, NULL, &cfStringSize, &resultCF );
	CI_ASSERT( status == noErr );

	string result = cocoa::convertCfString( resultCF );
	CFRelease( resultCF );
	return result;
}

size_t deviceNumChannels( ::AudioObjectID objectId, bool isInput )
{
	::AudioObjectPropertyAddress streamConfigProperty = audioObjectProperty( kAudioDevicePropertyStreamConfiguration, isInput ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput );
	UInt32 streamConfigPropertySize = audioObjectPropertyDataSize( objectId, streamConfigProperty );
	shared_ptr<::AudioBufferList> bufferList( (::AudioBufferList *)calloc( 1, streamConfigPropertySize ), free );

	audioObjectPropertyData( objectId, streamConfigProperty, streamConfigPropertySize,  bufferList.get() );

	size_t numChannels = 0;
	for( int i = 0; i < bufferList->mNumberBuffers; i++ ) {
		numChannels += bufferList->mBuffers[i].mNumberChannels;
	}
	return numChannels;
}

} // namespace audio2