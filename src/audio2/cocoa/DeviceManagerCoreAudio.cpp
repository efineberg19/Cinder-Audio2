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

#include "audio2/cocoa/DeviceManagerCoreAudio.h"
#include "audio2/cocoa/DeviceAudioUnit.h"
#include "audio2/audio.h"

#include "audio2/Debug.h"

#include "cinder/cocoa/CinderCocoa.h"

using namespace std;
using namespace ci;

namespace audio2 { namespace cocoa {

// some private helpers, not sure yet how widely useful these are
::AudioObjectPropertyAddress audioObjectProperty( ::AudioObjectPropertySelector propertySelector, ::AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal );
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

	return findDeviceByKey( DeviceManagerCoreAudio::keyForDeviceId( defaultOutputId ) );
}

DeviceRef DeviceManagerCoreAudio::getDefaultInput()
{
	::AudioDeviceID defaultInputId;
	UInt32 propertySize = sizeof( defaultInputId );
	::AudioObjectPropertyAddress propertyAddress = audioObjectProperty( kAudioHardwarePropertyDefaultInputDevice );
	audioObjectPropertyData( kAudioObjectSystemObject, propertyAddress, propertySize, &defaultInputId );
	
	return findDeviceByKey( DeviceManagerCoreAudio::keyForDeviceId( defaultInputId ) );
}

DeviceRef DeviceManagerCoreAudio::findDeviceByName( const std::string &name )
{
	for( const auto& device : getDevices() ) {
		if( device->getName() == name )
			return device;
	}

	LOG_E << "unknown device name: " << name << endl;
	return DeviceRef();
}

DeviceRef DeviceManagerCoreAudio::findDeviceByKey( const std::string &key )
{
	for( const auto& device : getDevices() ) {
		if( device->getKey() == key )
			return device;
	}

	LOG_E << "unknown device key: " << key << endl;
	return DeviceRef();
}

void DeviceManagerCoreAudio::setActiveDevice( const string &key )
{
	for( const auto& device : getDevices() ) {
		if( device->getKey() == key ) {
			auto deviceAU = dynamic_pointer_cast<DeviceAudioUnit>( device );
			auto idIt = mDeviceIds.find( device );
			CI_ASSERT( idIt != mDeviceIds.end() );

			::AudioDeviceID deviceId = idIt->second;
			OSStatus status = ::AudioUnitSetProperty( deviceAU->getComponentInstance(), kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &deviceId, sizeof( deviceId ) );
			CI_ASSERT( status == noErr );

			return;
		}
	}

	CI_ASSERT( 0 && "unreachable" );
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

size_t DeviceManagerCoreAudio::getNumFramesPerBlock( const string &key )
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

::AudioDeviceID DeviceManagerCoreAudio::getDeviceId( const std::string &key )
{
	for( const auto& devicePair : mDeviceIds ) {
		if( devicePair.first->getKey() == key )
			return devicePair.second;
	}

	CI_ASSERT( 0 && "unreachable" );
}

const std::vector<DeviceRef>& DeviceManagerCoreAudio::getDevices()
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
			auto device = DeviceRef( new DeviceAudioUnit( key, component ) );
			mDevices.push_back( device );
			mDeviceIds.insert( { device, deviceId } );
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

	string result = ci::cocoa::convertCfString( resultCF );
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

} } // namespace audio2::cocoa