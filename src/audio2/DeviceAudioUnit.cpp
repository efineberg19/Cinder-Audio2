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
	DeviceAudioUnitImpl( shared_ptr<DeviceInfo> info ) : mDeviceInfo( info )	{}

	static std::string keyForDeviceID( AudioDeviceID deviceID );

	shared_ptr<DeviceInfo> mDeviceInfo;
};

// some private helpers, not sure yet how widely useful these are
// TODO: get iOS running and see how it compares with Mac

AudioObjectPropertyAddress audioObjectProperty( AudioObjectPropertySelector propertySelector, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal );
UInt32 audioObjectPropertyDataSize( AudioDeviceID deviceID, const AudioObjectPropertyAddress& address, UInt32 qualifierDataSize = 0, const void* qualifierData = NULL );
string audioObjectPropertyString( AudioDeviceID deviceID, AudioObjectPropertySelector propertySelector );
size_t deviceNumChannels( AudioDeviceID deviceID, bool isInput );

// ----------------------------------------------------------------------------------------------------
// MARK: - OutputDeviceAudioUnit
// ----------------------------------------------------------------------------------------------------

OutputDeviceAudioUnit::OutputDeviceAudioUnit( shared_ptr<DeviceInfo> info )
: mImpl( new DeviceAudioUnitImpl( info ) )
{

}

OutputDeviceAudioUnit::~OutputDeviceAudioUnit()
{
}

void OutputDeviceAudioUnit::initialize()
{

}

void OutputDeviceAudioUnit::uninitialize()
{

}

void OutputDeviceAudioUnit::start()
{

}

void OutputDeviceAudioUnit::stop()
{

}

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManagerAudioUnit
// ----------------------------------------------------------------------------------------------------

OutputDeviceRef DeviceManagerAudioUnit::getDefaultOutput()
{
	::AudioDeviceID defaultOutputID;
	UInt32 propertySize = sizeof( defaultOutputID );
	::AudioObjectPropertyAddress propertyAddress; // TODO: replace with helper
	propertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
	propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
	propertyAddress.mElement = kAudioObjectPropertyElementMaster;
	OSStatus status = ::AudioObjectGetPropertyData( kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize, &defaultOutputID );
	CI_ASSERT( status == noErr );

	return OutputDeviceRef( new OutputDeviceAudioUnit( getDeviceInfo( defaultOutputID ) ) );
}

InputDeviceRef DeviceManagerAudioUnit::getDefaultInput()
{
	throw "not implemented";
	return InputDeviceRef();
}

shared_ptr<DeviceInfo> DeviceManagerAudioUnit::getDeviceInfo( ::AudioDeviceID deviceID )
{
	string key = DeviceAudioUnitImpl::keyForDeviceID( deviceID );
	DeviceInfoMap& deviceMap = getDevices();
	auto deviceIt = deviceMap.find( key );
	if( deviceIt != deviceMap.end() ) {
		return deviceIt->second;
	}
	throw AudioDeviceExc( "Could not find device by AudioDeviceID" ); // TODO: move this into if, return goes here
}


DeviceManagerAudioUnit::DeviceInfoMap& DeviceManagerAudioUnit::getDevices()
{
	if( mDevices.empty() ) {
		vector<::AudioObjectID> deviceIDs;
		UInt32 devicesPropertySize;
		::AudioObjectPropertyAddress devicesProperty = audioObjectProperty( kAudioHardwarePropertyDevices );

		// TODO: replace with helpers
		OSStatus status = ::AudioObjectGetPropertyDataSize( kAudioObjectSystemObject, &devicesProperty, 0, NULL, &devicesPropertySize );
		CI_ASSERT( status == noErr );

		size_t numDevices = devicesPropertySize / sizeof( AudioDeviceID );
		deviceIDs.resize( numDevices );

		status = ::AudioObjectGetPropertyData( kAudioObjectSystemObject, &devicesProperty, 0, NULL, &devicesPropertySize, deviceIDs.data() );
		CI_ASSERT( status == noErr );

		for ( AudioDeviceID &deviceID : deviceIDs ) {
			string key = DeviceAudioUnitImpl::keyForDeviceID( deviceID );

			auto devInfo = make_shared<DeviceInfo>();
			devInfo->mDeviceID = deviceID;
			devInfo->mNumInputChannels = deviceNumChannels( deviceID, true );
			devInfo->mNumOutputChannels = deviceNumChannels( deviceID, false );

			auto result = mDevices.insert( make_pair( key,  devInfo ) );
			CI_ASSERT( result.second );
		}

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

AudioObjectPropertyAddress audioObjectProperty( ::AudioObjectPropertySelector propertySelector, ::AudioObjectPropertyScope scope )
{
	::AudioObjectPropertyAddress result;
	result.mSelector = propertySelector;
	result.mScope = scope;
	result.mElement = kAudioObjectPropertyElementMaster;
	return result;
}

UInt32 audioObjectPropertyDataSize( AudioDeviceID deviceID, const ::AudioObjectPropertyAddress& propertyAddress, UInt32 qualifierDataSize, const void* qualifierData )
{
	UInt32 result = 0;
	OSStatus status = ::AudioObjectGetPropertyDataSize( deviceID, &propertyAddress, qualifierDataSize, qualifierData, &result );
	CI_ASSERT( status == noErr );

	return result;
}

string audioObjectPropertyString( AudioDeviceID deviceID, ::AudioObjectPropertySelector propertySelector )
{
	::AudioObjectPropertyAddress property = audioObjectProperty( propertySelector );
	if( !::AudioObjectHasProperty( deviceID, &property ) )
		return string();

	CFStringRef resultCF;
	UInt32 cfStringSize = sizeof( CFStringRef );

	OSStatus status = ::AudioObjectGetPropertyData( deviceID, &property, 0, NULL, &cfStringSize, &resultCF );
	CI_ASSERT( status == noErr );

	string result = cocoa::convertCfString( resultCF );
	CFRelease( resultCF );
	return result;
}

size_t deviceNumChannels( AudioDeviceID deviceID, bool isInput )
{
	::AudioObjectPropertyAddress streamConfigProperty = audioObjectProperty( kAudioDevicePropertyStreamConfiguration, isInput ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput );

	UInt32 streamConfigPropertySize = audioObjectPropertyDataSize( deviceID, streamConfigProperty );
	shared_ptr<::AudioBufferList> bufferList( (::AudioBufferList *)calloc( 1, streamConfigPropertySize ), free );
	OSStatus status = ::AudioObjectGetPropertyData( deviceID, &streamConfigProperty, 0, NULL, &streamConfigPropertySize, bufferList.get() );
	CI_ASSERT( status == noErr );

	size_t numChannels = 0;
	for( int i = 0; i < bufferList->mNumberBuffers; i++ ) {
		numChannels += bufferList->mBuffers[i].mNumberChannels;
	}
	return numChannels;
}

} // namespace audio2
