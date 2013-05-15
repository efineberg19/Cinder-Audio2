#pragma once

#include "audio2/Device.h"

#include <vector>

namespace audio2 {

class DeviceManagerMsw : public DeviceManager {
  public:
	DeviceRef getDefaultOutput() override;
	DeviceRef getDefaultInput() override;

	std::string getName( const std::string &key ) override;
	size_t getNumInputChannels( const std::string &key ) override;
	size_t getNumOutputChannels( const std::string &key ) override;
	size_t getSampleRate( const std::string &key ) override;
	size_t getBlockSize( const std::string &key ) override;

	void setActiveDevice( const std::string &key ) override;
  private:

	  // TODO: this is suitable for the base class and public API
	  // - but need to consider both how to get device by unique key and human-readable name
	  DeviceRef getDevice( const std::string &key );

	  struct DeviceInfo {
		  std::string			key;
		  //::AudioDeviceID		deviceID;
		  DeviceRef			device;
	  };
	  typedef std::vector<DeviceInfo> DeviceContainerT;
	  DeviceContainerT& getDevices();

	  DeviceContainerT mDevices;
};

} // namespace audio2