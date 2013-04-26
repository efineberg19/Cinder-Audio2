#pragma once

#include "audio2/Device.h"

#include <AudioToolbox/AudioToolbox.h>
#include <map>

namespace audio2 {

struct DeviceAudioUnitImpl;

class OutputDeviceAudioUnit : public OutputDevice {
  public:
	virtual ~OutputDeviceAudioUnit();

	void initialize() override;
	void uninitialize() override;

  private:
	std::unique_ptr<DeviceAudioUnitImpl> mImpl;
};

class InputDeviceAudioUnit : public InputDevice {
};

class DeviceManagerAudioUnit : public DeviceManager {
  public:
	OutputDeviceRef getDefaultOutput() override;
	InputDeviceRef getDefaultInput() override;

	std::shared_ptr<Device> getDevice( ::AudioDeviceID deviceID );

	typedef std::map<std::string, std::shared_ptr<Device> > DeviceMap;
	DeviceMap& getDeviceMap();
  private:
	DeviceMap mDevices;
};

} // namespace audio2
