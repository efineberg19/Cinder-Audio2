#pragma once

#include "audio2/Device.h"

#include <string>

namespace audio2 {

struct DeviceAudioUnitImpl;

class DeviceAudioUnit : public Device {
  public:
	virtual ~DeviceAudioUnit();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

  private:
	DeviceAudioUnit( const std::string &key );
	std::unique_ptr<DeviceAudioUnitImpl> mImpl;
	std::string mKey;

	friend class DeviceManagerCoreAudio; // ???: friend DeviceManagers here or can be avoided?
};

} // namespace audio2
