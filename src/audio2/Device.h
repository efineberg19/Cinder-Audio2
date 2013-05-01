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

	const std::string& getName();
	size_t getNumInputChannels();
	size_t getNumOutputChannels();
	size_t getSampleRate();
	size_t getBlockSize();

  protected:
	Device( const std::string &key ) : mKey( key ), mInitialized( false ), mRunning( false ) {}

	bool mInitialized, mRunning;
	std::string mKey, mName;
};

class DeviceManager {
  public:
	virtual ~DeviceManager() {}
	virtual DeviceRef getDefaultOutput() = 0;
	virtual DeviceRef getDefaultInput() = 0;

	virtual std::string getName( const std::string &key ) = 0;
	virtual size_t getNumInputChannels( const std::string &key ) = 0;
	virtual size_t getNumOutputChannels( const std::string &key ) = 0;
	virtual size_t getSampleRate( const std::string &key ) = 0;
	virtual size_t getBlockSize( const std::string &key ) = 0;

	virtual void setActiveDevice( const std::string &key ) = 0;

	static DeviceManager* instance();

protected:
	DeviceManager() = default;
};

} // namespace audio2