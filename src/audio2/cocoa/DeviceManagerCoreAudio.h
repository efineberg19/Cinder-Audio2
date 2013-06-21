#pragma once

#include "audio2/Device.h"

#include <CoreAudio/CoreAudio.h>
#include <map>

namespace audio2 { namespace cocoa {

class DeviceManagerCoreAudio : public DeviceManager {
public:

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

  private:

	::AudioDeviceID getDeviceId( const std::string &key );
	static std::string keyForDeviceId( ::AudioObjectID deviceId );

	std::map<DeviceRef,::AudioDeviceID> mDeviceIds;
};

} } // audio2::cocoa