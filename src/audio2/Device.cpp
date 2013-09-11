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

#include "audio2/Context.h"
#include "audio2/Device.h"
#include "audio2/Debug.h"

#if defined( CINDER_COCOA )
	#if defined( CINDER_MAC )
		#include "audio2/cocoa/DeviceManagerCoreAudio.h"
	#else
		#include "audio2/cocoa/DeviceManagerAudioSession.h"
	#endif
#elif defined( CINDER_MSW )
	#include "audio2/msw/DeviceManagerWasapi.h"
#endif

using namespace std;

namespace cinder { namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - Device
// ----------------------------------------------------------------------------------------------------

DeviceRef Device::getDefaultOutput()
{
	return Context::deviceManager()->getDefaultOutput();
}

DeviceRef Device::getDefaultInput()
{
	return Context::deviceManager()->getDefaultInput();
}

DeviceRef Device::findDeviceByName( const string &name )
{
	return Context::deviceManager()->findDeviceByName( name );
}

DeviceRef Device::findDeviceByKey( const string &key )
{
	return Context::deviceManager()->findDeviceByKey( key );
}

const vector<DeviceRef>& Device::getDevices()
{
	return Context::deviceManager()->getDevices();
}

const string& Device::getName()
{
	if( mName.empty() )
		mName = Context::deviceManager()->getName( mKey );

	return mName;
}

const string& Device::getKey()
{
	return mKey;
}

size_t Device::getNumInputChannels()
{
	return Context::deviceManager()->getNumInputChannels( mKey );
}

size_t Device::getNumOutputChannels()
{
	return Context::deviceManager()->getNumOutputChannels( mKey );
}

size_t Device::getSampleRate()
{
	return Context::deviceManager()->getSampleRate( mKey );
}

size_t Device::getFramesPerBlock()
{
	return Context::deviceManager()->getFramesPerBlock( mKey );
}

void Device::setSampleRate( size_t sampleRate )
{
	Context::deviceManager()->setSampleRate( mKey, sampleRate );
}

void Device::setFramesPerBlock( size_t framesPerBlock )
{
	Context::deviceManager()->setFramesPerBlock( mKey, framesPerBlock );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManager
// ----------------------------------------------------------------------------------------------------

DeviceRef DeviceManager::findDeviceByName( const string &name )
{
	for( const auto &device : getDevices() ) {
		if( device->getName() == name )
			return device;
	}

	LOG_E << "unknown device name: " << name << endl;
	return DeviceRef();
}

DeviceRef DeviceManager::findDeviceByKey( const string &key )
{
	for( const auto &device : getDevices() ) {
		if( device->getKey() == key )
			return device;
	}

	LOG_E << "unknown device key: " << key << endl;
	return DeviceRef();
}

DeviceRef DeviceManager::addDevice( const string &key )
{
	for( const auto& dev : mDevices ) {
		if( dev->getKey() == key ) {
			LOG_E << "device already exists with key: " << key << endl;
			return DeviceRef();
		}
	}

	mDevices.push_back( DeviceRef( new Device( key ) ) );
	return mDevices.back();
}

} } // namespace cinder::audio2