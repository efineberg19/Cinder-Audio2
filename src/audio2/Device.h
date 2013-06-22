#pragma once

#include <memory>
#include <string>
#include <vector>

namespace audio2 {

typedef std::shared_ptr<class Device> DeviceRef;

class Device {
  public:
	static DeviceRef getDefaultOutput();
	static DeviceRef getDefaultInput();
	static DeviceRef findDeviceByName( const std::string &name );
	static DeviceRef findDeviceByKey( const std::string &key );


	static const std::vector<DeviceRef>& getDevices();
	virtual ~Device() {}

	virtual void initialize() = 0;
	virtual void uninitialize() = 0;

	virtual void start() = 0;
	virtual void stop() = 0;

	const std::string& getName();
	const std::string& getKey();
	size_t getNumInputChannels();
	size_t getNumOutputChannels();
	size_t getSampleRate();
	size_t getNumFramesPerBlock();

  protected:
	Device( const std::string &key ) : mKey( key ), mInitialized( false ), mEnabled( false ) {}

	bool mInitialized, mEnabled;
	std::string mKey, mName;
};

class DeviceManager {
  public:
	virtual ~DeviceManager() {}
	virtual DeviceRef getDefaultOutput() = 0;
	virtual DeviceRef getDefaultInput() = 0;

	virtual DeviceRef findDeviceByName( const std::string &name ) = 0;
	virtual DeviceRef findDeviceByKey( const std::string &key ) = 0;

	virtual const std::vector<DeviceRef>& getDevices() = 0;

	virtual std::string getName( const std::string &key ) = 0;
	virtual size_t getNumInputChannels( const std::string &key ) = 0;
	virtual size_t getNumOutputChannels( const std::string &key ) = 0;
	virtual size_t getSampleRate( const std::string &key ) = 0;
	virtual size_t getNumFramesPerBlock( const std::string &key ) = 0;

	// TODO: the functionality in this method feels awkward, consider doing it in device
	// - for iOS audio session activating, can just do that in DeviceManager's constructor
	virtual void setActiveDevice( const std::string &key ) = 0;

	static DeviceManager* instance();

protected:
	DeviceManager()	{}

	std::vector<DeviceRef> mDevices;
};

} // namespace audio2