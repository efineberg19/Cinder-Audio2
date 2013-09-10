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

#include "audio2/Device.h"
#include "audio2/audio.h"
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
	return DeviceManager::instance()->getDefaultOutput();
}

DeviceRef Device::getDefaultInput()
{
	return DeviceManager::instance()->getDefaultInput();
}

DeviceRef Device::findDeviceByName( const string &name )
{
	return DeviceManager::instance()->findDeviceByName( name );
}

DeviceRef Device::findDeviceByKey( const string &key )
{
	return DeviceManager::instance()->findDeviceByKey( key );
}

const vector<DeviceRef>& Device::getDevices()
{
	return DeviceManager::instance()->getDevices();
}

const string& Device::getName()
{
	if( mName.empty() )
		mName = DeviceManager::instance()->getName( mKey );

	return mName;
}

const string& Device::getKey()
{
	return mKey;
}

size_t Device::getNumInputChannels()
{
	return DeviceManager::instance()->getNumInputChannels( mKey );
}

size_t Device::getNumOutputChannels()
{
	return DeviceManager::instance()->getNumOutputChannels( mKey );
}

size_t Device::getSampleRate()
{
	return DeviceManager::instance()->getSampleRate( mKey );
}

size_t Device::getFramesPerBlock()
{
	return DeviceManager::instance()->getFramesPerBlock( mKey );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManager
// ----------------------------------------------------------------------------------------------------

// TODO: it would be nice for this to live in Context, where there is already other platform-specific
// variants being chosen.
// - I'm thinking to move it to Context::sDeviceManager, retrievable via Context::getDeviceManager()
// - also provide Context::setDeviceManager(DeviceManager *), in the rare case that users want a custom version
// - ???: does this improve anything over the current situation, if I add the same methods to DeviceManager?
DeviceManager* DeviceManager::instance()
{
	static DeviceManager *sInstance = 0;
	if( ! sInstance ) {
#if defined( CINDER_MAC )
		sInstance = new cocoa::DeviceManagerCoreAudio();
#elif defined( CINDER_COCOA_TOUCH )
		sInstance = new cocoa::DeviceManagerAudioSession();
#elif defined( CINDER_MSW )
		sInstance = new msw::DeviceManagerWasapi();
#endif
	}
	return sInstance;
}

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