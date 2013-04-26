#pragma once

#include <memory>

namespace audio2 {

typedef std::shared_ptr<class OutputDevice> OutputDeviceRef;
typedef std::shared_ptr<class InputDevice> InputDeviceRef;

class Device {
  public:

	virtual void initialize() = 0;
	virtual void uninitialize() = 0;

	virtual void start() = 0;
	virtual void stop() = 0;

	size_t	getNumChannels() const;
	size_t	getSampleRate() const;
};

class OutputDevice : public Device {
  public:
	static OutputDeviceRef getDefault();
};

class InputDevice : public Device {
  public:
};

class DeviceManager {
  public:
	virtual ~DeviceManager() {}
	virtual OutputDeviceRef getDefaultOutput() = 0;
	virtual InputDeviceRef getDefaultInput() = 0;

	static DeviceManager* instance();
};

} // namespace audio2