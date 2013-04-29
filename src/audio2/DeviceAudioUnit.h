#pragma once

#include "audio2/Device.h"

#include <string>

namespace audio2 {

struct DeviceAudioUnitImpl;

class OutputDeviceAudioUnit : public OutputDevice {
  public:
	virtual ~OutputDeviceAudioUnit();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

  private:
	OutputDeviceAudioUnit( const std::string &key );
	std::unique_ptr<DeviceAudioUnitImpl> mImpl;
	std::string mKey;

	friend class DeviceManagerCoreAudio; // ???: friend DeviceManagers here or can be avoided?
};

class InputDeviceAudioUnit : public InputDevice {
};

} // namespace audio2
