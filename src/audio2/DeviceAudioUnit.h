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

} // namespace audio2