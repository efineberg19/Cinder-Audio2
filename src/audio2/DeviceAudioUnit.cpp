#include "audio2/DeviceAudioUnit.h"
#include "audio2/audio.h"
#include "audio2/assert.h"

#include "cinder/cocoa/CinderCocoa.h"

using namespace std;
using namespace ci;

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceAudioUnitImpl Declaration
// ----------------------------------------------------------------------------------------------------

struct DeviceAudioUnitImpl {

	static std::string keyForDeviceID( AudioDeviceID deviceID );
};

	// some private helpers, not sure yet how widely useful these are
	// TODO: get iOS running and see how it compares with Mac

	AudioObjectPropertyAddress audioObjectProperty( AudioObjectPropertySelector propertySelector );
	string audioObjectPropertyString( AudioDeviceID deviceID, AudioObjectPropertySelector propertySelector );


// ----------------------------------------------------------------------------------------------------
// MARK: - OutputDeviceAudioUnit
// ----------------------------------------------------------------------------------------------------

OutputDeviceAudioUnit::~OutputDeviceAudioUnit()
{
}

void OutputDeviceAudioUnit::initialize()
{

}

void OutputDeviceAudioUnit::uninitialize()
{

}

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManagerAudioUnit
// ----------------------------------------------------------------------------------------------------

OutputDeviceRef DeviceManagerAudioUnit::getDefaultOutput()
{
	::AudioDeviceID defaultOutput;
	UInt32 propertySize = sizeof( defaultOutput );
	::AudioObjectPropertyAddress propertyAddress;
	propertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
	propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
	propertyAddress.mElement = kAudioObjectPropertyElementMaster;
	OSStatus status = ::AudioObjectGetPropertyData( kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize, &defaultOutput );
	CI_ASSERT( status == noErr );

	return static_pointer_cast<OutputDevice>( getDevice( defaultOutput ) );
}

InputDeviceRef DeviceManagerAudioUnit::getDefaultInput()
{
	throw "not implemented";
	return InputDeviceRef();
}

shared_ptr<Device> DeviceManagerAudioUnit::getDevice( ::AudioDeviceID deviceID )
{
	string key = DeviceAudioUnitImpl::keyForDeviceID( deviceID );
	DeviceMap& deviceMap = getDeviceMap();
	auto deviceIt = deviceMap.find( key );
	if( deviceIt != deviceMap.end() ) {
		return deviceIt->second;
	}
	throw AudioDeviceExc( "Could not find device by AudioDeviceID" );
}


DeviceManagerAudioUnit::DeviceMap& DeviceManagerAudioUnit::getDeviceMap()
{
	if( mDevices.empty() ) {
		vector<AudioObjectID> deviceIDs;
		UInt32 devicesPropertySize;
		AudioObjectPropertyAddress devicesProperty = audioObjectProperty( kAudioHardwarePropertyDevices );

		OSStatus status = AudioObjectGetPropertyDataSize( kAudioObjectSystemObject, &devicesProperty, 0, NULL, &devicesPropertySize );
		CI_ASSERT( status == noErr );

		size_t numDevices = devicesPropertySize / sizeof( AudioDeviceID );
		deviceIDs.resize( numDevices );

		status = AudioObjectGetPropertyData( kAudioObjectSystemObject, &devicesProperty, 0, NULL, &devicesPropertySize, deviceIDs.data() );
		CI_ASSERT( status == noErr );

		// TODO NEXT: build device map
		//	- I was adding an empty device before for this, which is how the I/O with one device was possible
		//  - the device is not initialized here, that happens when it is going to be used.
//		for ( AudioDeviceID &deviceID : deviceIDs ) {
//			CAHALAudioDevice caDevice( deviceID );
//			string key = DeviceAudioUnitImpl::keyForDeviceID( deviceID );
//			auto result = mDevices.insert( make_pair( key, AudioDeviceRef( new AudioDevice( deviceID ) ) ) );
//			CI_ASSERT( result.second );
//		}

	}
	return mDevices;
}


// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceAudioUnitImpl
// ----------------------------------------------------------------------------------------------------

	// note: we cannot just rely on 'model UID', when it is there (which it isn't always), becasue it can be the same
	// for two different 'devices', such as system input and output
	// - current solution: key = 'NAME-[UID | MANUFACTURE]'
	std::string DeviceAudioUnitImpl::keyForDeviceID( AudioDeviceID deviceID )
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

AudioObjectPropertyAddress audioObjectProperty( AudioObjectPropertySelector propertySelector ) {
	::AudioObjectPropertyAddress result;
	result.mSelector = propertySelector;
	result.mScope = kAudioObjectPropertyScopeGlobal;
	result.mElement = kAudioObjectPropertyElementMaster;
	return result;
}

string audioObjectPropertyString( AudioDeviceID deviceID, AudioObjectPropertySelector propertySelector )
{
	::AudioObjectPropertyAddress property = audioObjectProperty( propertySelector );
	if( !AudioObjectHasProperty( deviceID, &property ) )
		return string();

	CFStringRef resultCF;
	UInt32 cfStringSize = sizeof( CFStringRef );

	OSStatus status = AudioObjectGetPropertyData( deviceID, &property, 0, NULL, &cfStringSize, &resultCF );
	CI_ASSERT( status == noErr );

	string result = cocoa::convertCfString( resultCF );
	CFRelease( resultCF );
	return result;
}

} // namespace audio2
