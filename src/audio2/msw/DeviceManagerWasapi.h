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

struct IMMDevice;

namespace cinder { namespace audio2 { namespace msw {

// TODO: minimal DeviceManagerXp

class DeviceManagerWasapi : public DeviceManager {
  public:
	DeviceRef getDefaultOutput() override;
	DeviceRef getDefaultInput() override;

	const std::vector<DeviceRef>& getDevices() override;

	std::string getName( const std::string &key ) override;
	size_t getNumInputChannels( const std::string &key ) override;
	size_t getNumOutputChannels( const std::string &key ) override;
	size_t getSampleRate( const std::string &key ) override;
	size_t getFramesPerBlock( const std::string &key ) override;

	void setActiveDevice( const std::string &key ) override;

	const std::wstring& getDeviceId( const std::string &key );

	std::shared_ptr<::IMMDevice> getIMMDevice( const std::string &key );

  private:

	  struct DeviceInfo {
		  std::string key;						//! key used by Device to get more info from manager
		  std::string name;						//! friendly name
		  enum Usage { Input, Output } usage;
		  std::wstring			deviceId;		//! id used when creating XAudio2 master voice
		  std::wstring			endpointId;		//! id used by Wasapi / MMDevice
		  size_t numChannels, sampleRate;
	  };

	  DeviceInfo& getDeviceInfo( const std::string &key );
	  void parseDevices( DeviceInfo::Usage usage );

	  std::vector<DeviceInfo> mDeviceInfoArray;
};

} } } // namespace cinder::audio2::msw