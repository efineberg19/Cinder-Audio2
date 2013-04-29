#pragma once

#include <memory>

namespace audio2 {

typedef std::shared_ptr<class Device> DeviceRef;

class Device {
  public:
	static DeviceRef getDefaultOutput();

	virtual void initialize() = 0;
	virtual void uninitialize() = 0;

	virtual void start() = 0;
	virtual void stop() = 0;

	size_t	getNumChannels() const;
	size_t	getSampleRate() const;
};

class DeviceManager {
  public:
	virtual ~DeviceManager() {}
	virtual DeviceRef getDefaultOutput() = 0;
	virtual DeviceRef getDefaultInput() = 0;

	static DeviceManager* instance();
};

} // namespace audio2