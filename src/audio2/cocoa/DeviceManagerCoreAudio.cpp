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
#include "audio2/audio.h"

#include "audio2/Debug.h"

#include "cinder/cocoa/CinderCocoa.h"

using namespace std;
using namespace ci;

namespace cinder { namespace audio2 { namespace cocoa {

// some private helpers, not sure yet how widely useful these are
::AudioObjectPropertyAddress getAudioObjectProperty( ::AudioObjectPropertySelector propertySelector, ::AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal );
UInt32 getAudioObjectPropertyDataSize( ::AudioObjectID objectId, const AudioObjectPropertyAddress &address, UInt32 qualifierDataSize = 0, const void *qualifierData = NULL );
string getAudioObjectPropertyString( ::AudioObjectID objectId, AudioObjectPropertySelector propertySelector );
void getAudioObjectPropertyData( ::AudioObjectID objectId, const ::AudioObjectPropertyAddress& propertyAddress, UInt32 dataSize, void *data, UInt32 qualifierDataSize = 0, const void *qualifierData = NULL );
size_t getAudioObjectNumChannels( ::AudioDeviceID objectId, bool isInput );

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManagerCoreAudio
// ----------------------------------------------------------------------------------------------------

DeviceRef DeviceManagerCoreAudio::getDefaultOutput()
{
	::AudioDeviceID defaultOutputId;
	UInt32 propertySize = sizeof( defaultOutputId );
	::AudioObjectPropertyAddress propertyAddress = getAudioObjectProperty( kAudioHardwarePropertyDefaultOutputDevice );
	getAudioObjectPropertyData( kAudioObjectSystemObject, propertyAddress, propertySize, &defaultOutputId );

	return findDeviceByKey( DeviceManagerCoreAudio::keyForDeviceId( defaultOutputId ) );
}

DeviceRef DeviceManagerCoreAudio::getDefaultInput()
{
	::AudioDeviceID defaultInputId;
	UInt32 propertySize = sizeof( defaultInputId );
	::AudioObjectPropertyAddress propertyAddress = getAudioObjectProperty( kAudioHardwarePropertyDefaultInputDevice );
	getAudioObjectPropertyData( kAudioObjectSystemObject, propertyAddress, propertySize, &defaultInputId );
	
	return findDeviceByKey( DeviceManagerCoreAudio::keyForDeviceId( defaultInputId ) );
}

void DeviceManagerCoreAudio::setCurrentDevice( const string &key, ::AudioComponentInstance componentInstance )
{
	for( const auto& device : getDevices() ) {
		if( device->getKey() == key ) {
			auto idIt = mDeviceIds.find( device );
			CI_ASSERT( idIt != mDeviceIds.end() );

			::AudioDeviceID deviceId = idIt->second;
			OSStatus status = ::AudioUnitSetProperty( componentInstance, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &deviceId, sizeof( deviceId ) );
			CI_ASSERT( status == noErr );

			return;
		}
	}

	CI_ASSERT( 0 && "unreachable" );
}

std::string DeviceManagerCoreAudio::getName( const string &key )
{
	::AudioDeviceID deviceId = getDeviceId( key );
	return getAudioObjectPropertyString( deviceId, kAudioObjectPropertyName );
}

size_t DeviceManagerCoreAudio::getNumInputChannels( const string &key )
{
	::AudioDeviceID deviceId = getDeviceId( key );
	return getAudioObjectNumChannels( deviceId, true );
}

size_t DeviceManagerCoreAudio::getNumOutputChannels( const string &key )
{
	::AudioDeviceID deviceId = getDeviceId( key );
	return getAudioObjectNumChannels( deviceId, false );
}

size_t DeviceManagerCoreAudio::getSampleRate( const string &key )
{
	::AudioDeviceID deviceId = getDeviceId( key );
	::AudioObjectPropertyAddress property = getAudioObjectProperty( kAudioDevicePropertyActualSampleRate );
	Float64 result;
	UInt32 resultSize = sizeof( result );

	getAudioObjectPropertyData( deviceId, property, resultSize, &result );
	return static_cast<size_t>( result );
}

size_t DeviceManagerCoreAudio::getFramesPerBlock( const string &key )
{
	::AudioDeviceID deviceId = getDeviceId( key );
	::AudioObjectPropertyAddress property = getAudioObjectProperty( kAudioDevicePropertyBufferFrameSize );
	UInt32 result;
	UInt32 resultSize = sizeof( result );

	getAudioObjectPropertyData( deviceId, property, resultSize, &result );
	return static_cast<size_t>( result );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - Private
// ----------------------------------------------------------------------------------------------------

::AudioDeviceID DeviceManagerCoreAudio::getDeviceId( const std::string &key )
{
	CI_ASSERT( ! mDeviceIds.empty() );

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
		::AudioObjectPropertyAddress devicesProperty = getAudioObjectProperty( kAudioHardwarePropertyDevices );
		UInt32 devicesPropertySize = getAudioObjectPropertyDataSize( kAudioObjectSystemObject, devicesProperty );
		size_t numDevices = devicesPropertySize / sizeof( ::AudioDeviceID );
		deviceIds.resize( numDevices );

		getAudioObjectPropertyData( kAudioObjectSystemObject, devicesProperty, devicesPropertySize, deviceIds.data() );

		for ( ::AudioDeviceID &deviceId : deviceIds ) {
			string key = keyForDeviceId( deviceId );
			auto device = addDevice( key );
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
	string name = getAudioObjectPropertyString( deviceId, kAudioObjectPropertyName );
	string key = getAudioObjectPropertyString( deviceId, kAudioDevicePropertyModelUID );
	if( key.empty() )
		key = getAudioObjectPropertyString( deviceId, kAudioObjectPropertyManufacturer );

	return name + " - " + key;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - Core Audio Helpers
// ----------------------------------------------------------------------------------------------------


AudioObjectPropertyAddress getAudioObjectProperty( ::AudioObjectPropertySelector propertySelector, ::AudioObjectPropertyScope scope )
{
	::AudioObjectPropertyAddress result;
	result.mSelector = propertySelector;
	result.mScope = scope;
	result.mElement = kAudioObjectPropertyElementMaster;
	return result;
}

UInt32 getAudioObjectPropertyDataSize( ::AudioObjectID objectId, const ::AudioObjectPropertyAddress &propertyAddress, UInt32 qualifierDataSize, const void *qualifierData )
{
	UInt32 result = 0;
	OSStatus status = ::AudioObjectGetPropertyDataSize( objectId, &propertyAddress, qualifierDataSize, qualifierData, &result );
	CI_ASSERT( status == noErr );

	return result;
}

void getAudioObjectPropertyData( ::AudioObjectID objectId, const ::AudioObjectPropertyAddress& propertyAddress, UInt32 dataSize, void *data, UInt32 qualifierDataSize, const void* qualifierData )
{
	OSStatus status = ::AudioObjectGetPropertyData( objectId, &propertyAddress, qualifierDataSize, qualifierData, &dataSize, data );
	CI_ASSERT( status == noErr );
}

string getAudioObjectPropertyString( ::AudioObjectID objectId, ::AudioObjectPropertySelector propertySelector )
{
	::AudioObjectPropertyAddress property = getAudioObjectProperty( propertySelector );
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

size_t getAudioObjectNumChannels( ::AudioObjectID objectId, bool isInput )
{
	::AudioObjectPropertyAddress streamConfigProperty = getAudioObjectProperty( kAudioDevicePropertyStreamConfiguration, isInput ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput );
	UInt32 streamConfigPropertySize = getAudioObjectPropertyDataSize( objectId, streamConfigProperty );
	shared_ptr<::AudioBufferList> bufferList( (::AudioBufferList *)calloc( 1, streamConfigPropertySize ), free );

	getAudioObjectPropertyData( objectId, streamConfigProperty, streamConfigPropertySize, bufferList.get() );

	size_t numChannels = 0;
	for( int i = 0; i < bufferList->mNumberBuffers; i++ ) {
		numChannels += bufferList->mBuffers[i].mNumberChannels;
	}
	return numChannels;
}

} } } // namespace cinder::audio2::cocoa