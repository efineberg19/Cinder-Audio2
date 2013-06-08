#pragma once

#include "audio2/Device.h"

#include <CoreAudio/CoreAudio.h>
#include <vector>

namespace audio2 { namespace cocoa {

class DeviceManagerCoreAudio : public DeviceManager {

	DeviceRef getDefaultOutput() override;
	DeviceRef getDefaultInput() override;

	DeviceRef getDevice( const std::string &key );

	std::string getName( const std::string &key ) override;
	size_t getNumInputChannels( const std::string &key ) override;
	size_t getNumOutputChannels( const std::string &key ) override;
	size_t getSampleRate( const std::string &key ) override;
	size_t getBlockSize( const std::string &key ) override;

	void setActiveDevice( const std::string &key ) override;

  private:

	static std::string keyForDeviceId( ::AudioObjectID deviceId );

	struct DeviceInfo {
		std::string			key;
		::AudioDeviceID		deviceId;
		DeviceRef			device;
	};
	typedef std::vector<DeviceInfo> DeviceContainerT;

	::AudioDeviceID getDeviceId( const std::string &key );
	DeviceContainerT& getDevices();

	DeviceContainerT mDevices;
};

} } // audio2::cocoa