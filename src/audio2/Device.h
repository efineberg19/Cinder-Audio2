#pragma once

#include <memory>
#include <string>

namespace audio2 {

typedef std::shared_ptr<class Device> DeviceRef;

class Device {
  public:
	static DeviceRef getDefaultOutput();
	virtual ~Device() {}

	virtual void initialize() = 0;
	virtual void uninitialize() = 0;

	virtual void start() = 0;
	virtual void stop() = 0;

	virtual const std::string& getName() = 0;

	size_t	getNumChannels() const;
	size_t	getSampleRate() const;

  protected:
	Device() : mInitialized( false ), mRunning( false ) {}

	bool mInitialized, mRunning;
};

class DeviceManager {
  public:
	virtual ~DeviceManager() {}
	virtual DeviceRef getDefaultOutput() = 0;
	virtual DeviceRef getDefaultInput() = 0;

	virtual void setActiveDevice( const std::string &key ) = 0;

	static DeviceManager* instance();
};

} // namespace audio2