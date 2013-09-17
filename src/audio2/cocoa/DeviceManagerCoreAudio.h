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

#pragma once

#include "audio2/Device.h"

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <map>

namespace cinder { namespace audio2 { namespace cocoa {

class DeviceManagerCoreAudio : public DeviceManager {
  public:
	const std::vector<DeviceRef>& getDevices()									override;
	DeviceRef getDefaultOutput()												override;
	DeviceRef getDefaultInput()													override;

	std::string getName( const std::string &key )								override;
	size_t getNumInputChannels( const std::string &key )						override;
	size_t getNumOutputChannels( const std::string &key )						override;
	size_t getSampleRate( const std::string &key )								override;
	size_t getFramesPerBlock( const std::string &key )							override;

	void setSampleRate( const std::string &key, size_t sampleRate )				override;
	void setFramesPerBlock( const std::string &key, size_t framesPerBlock )		override;

	//! Sets the device related to \a key and managed by \a componenetInstance as the current active audio device.
	void setCurrentDevice( const DeviceRef &key, ::AudioComponentInstance componentInstance );

  private:
	void registerPropertyListeners( const DeviceRef &device, ::AudioDeviceID deviceId );
	void unregisterPropertyListeners( const DeviceRef &device, ::AudioDeviceID deviceId );

	::AudioDeviceID getDeviceId( const std::string &key );
	static std::string keyForDeviceId( ::AudioDeviceID deviceId );

	std::map<DeviceRef,::AudioDeviceID>		mDeviceIds;
	DeviceRef								mCurrentDevice;
	::AudioObjectPropertyListenerBlock		mPropertyListenerBlock;
};

} } } // namespace cinder::audio2::cocoa