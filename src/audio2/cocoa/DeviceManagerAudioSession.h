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

#include "audio2/audio.h"
#include "audio2/Device.h"

namespace audio2 { namespace cocoa {

class DeviceAudioUnit;

class DeviceManagerAudioSession : public DeviceManager {
  public:
	DeviceManagerAudioSession();
	virtual ~DeviceManagerAudioSession() = default;

	DeviceRef getDefaultOutput() override;
	DeviceRef getDefaultInput() override;
	DeviceRef findDeviceByName( const std::string &name ) override;
	DeviceRef findDeviceByKey( const std::string &key ) override;

	const std::vector<DeviceRef>& getDevices() override;

	std::string getName( const std::string &key ) override;
	size_t getNumInputChannels( const std::string &key ) override;
	size_t getNumOutputChannels( const std::string &key ) override;
	size_t getSampleRate( const std::string &key ) override;
	size_t getNumFramesPerBlock( const std::string &key ) override;

	void setActiveDevice( const std::string &key ) override;

	bool inputIsEnabled();

  private:

	std::shared_ptr<DeviceAudioUnit>	getRemoteIOUnit();
	void								activateSession();
	uint32_t							getSessionCategory(); // TODO: consider useing the strings provided by AVAudioSession


	std::shared_ptr<DeviceAudioUnit> mRemoteIOUnit;

	bool mSessionIsActive;
};

} } // audio2::cocoa