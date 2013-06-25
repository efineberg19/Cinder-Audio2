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

#include <memory>
#include <string>
#include <vector>

namespace audio2 {

typedef std::shared_ptr<class Device> DeviceRef;

class Device {
  public:
	static DeviceRef getDefaultOutput();
	static DeviceRef getDefaultInput();
	static DeviceRef findDeviceByName( const std::string &name );
	static DeviceRef findDeviceByKey( const std::string &key );


	static const std::vector<DeviceRef>& getDevices();
	virtual ~Device() {}

	virtual void initialize() = 0;
	virtual void uninitialize() = 0;

	virtual void start() = 0;
	virtual void stop() = 0;

	const std::string& getName();
	const std::string& getKey();
	size_t getNumInputChannels();
	size_t getNumOutputChannels();
	size_t getSampleRate();
	size_t getNumFramesPerBlock();

  protected:
	Device( const std::string &key ) : mKey( key ), mInitialized( false ), mEnabled( false ) {}

	bool mInitialized, mEnabled;
	std::string mKey, mName;
};

class DeviceManager {
  public:
	virtual ~DeviceManager() {}
	static DeviceManager* instance();

	virtual DeviceRef findDeviceByName( const std::string &name );
	virtual DeviceRef findDeviceByKey( const std::string &key );

	virtual const std::vector<DeviceRef>& getDevices() = 0;
	virtual DeviceRef getDefaultOutput() = 0;
	virtual DeviceRef getDefaultInput() = 0;

	virtual std::string getName( const std::string &key ) = 0;
	virtual size_t getNumInputChannels( const std::string &key ) = 0;
	virtual size_t getNumOutputChannels( const std::string &key ) = 0;
	virtual size_t getSampleRate( const std::string &key ) = 0;
	virtual size_t getNumFramesPerBlock( const std::string &key ) = 0;

	// TODO: the functionality in this method feels awkward, consider doing it in device
	// - for iOS audio session activating, can just do that in DeviceManager's constructor
	virtual void setActiveDevice( const std::string &key ) = 0;

protected:
	DeviceManager()	{}

	std::vector<DeviceRef> mDevices;
};

} // namespace audio2