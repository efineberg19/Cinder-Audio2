#pragma once

#include "audio2/Device.h"

#include <AudioToolbox/AudioToolbox.h>
#include <map>

namespace audio2 {

struct DeviceAudioUnitImpl;

// Need a piece of data that can be shared among OutputDevice and Input device, so we can tell
// if the underlining AudioComponentInstance is being using for simultaneous I/O
// ???: is this too fragmented? Move to Device, or DeviceAudioUnitImpl?
struct DeviceInfo {
	DeviceInfo() : mDeviceID( kAudioObjectUnknown ), mNumInputChannels( 0 ), mNumOutputChannels( 0 ), mInputConnected( false ), mOutputConnected( false )
	{}
	
	::AudioDeviceID mDeviceID;
	size_t mNumInputChannels, mNumOutputChannels;
	bool mInputConnected, mOutputConnected;
};

class OutputDeviceAudioUnit : public OutputDevice {
  public:
	virtual ~OutputDeviceAudioUnit();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

  private:
	OutputDeviceAudioUnit( std::shared_ptr<DeviceInfo> info );
	std::unique_ptr<DeviceAudioUnitImpl> mImpl;

	friend class DeviceManagerAudioUnit;
};

class InputDeviceAudioUnit : public InputDevice {
};

class DeviceManagerAudioUnit : public DeviceManager {
  public:
	OutputDeviceRef getDefaultOutput() override;
	InputDeviceRef getDefaultInput() override;

	typedef std::map<std::string, std::shared_ptr<DeviceInfo> > DeviceInfoMap;

  private:
	std::shared_ptr<DeviceInfo> getDeviceInfo( ::AudioDeviceID deviceID );

	DeviceInfoMap& getDevices();
	DeviceInfoMap mDevices;
};

} // namespace audio2
