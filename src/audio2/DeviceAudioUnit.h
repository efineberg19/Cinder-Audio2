#pragma once

#include "audio2/Device.h"

namespace audio2 {

struct DeviceAudioUnitImpl;

class OutputDeviceAudioUnit : public OutputDevice {
  public:
	virtual ~OutputDeviceAudioUnit();

  private:
	std::unique_ptr<DeviceAudioUnitImpl> mImpl;
};

class InputDeviceAudioUnit : public InputDevice {

};

class DeviceManagerAudioUnit : public DeviceManager {

	virtual OutputDeviceRef getDefaultOutput() override;
	virtual InputDeviceRef getDefaultInput() override;
	
};

} // namespace audio2