#pragma once

#include "audio2/Device.h"

#include <vector>

struct IMMDevice;

namespace audio2 {

// TODO: rename this DeviceManagerWasapi ?
//	- this one requires Wasapi, as such XP is a no-go
//  - but this also creates xaudio output
//  - should have another device manager for xp that just uses XAudio
class DeviceManagerMsw : public DeviceManager {
  public:
	DeviceRef getDefaultOutput() override;
	DeviceRef getDefaultInput() override;

	std::string getName( const std::string &key ) override;
	size_t getNumInputChannels( const std::string &key ) override;
	size_t getNumOutputChannels( const std::string &key ) override;
	size_t getSampleRate( const std::string &key ) override;
	size_t getBlockSize( const std::string &key ) override;

	void setActiveDevice( const std::string &key ) override;

	const std::wstring& getDeviceId( const std::string &key );

	::IMMDevice *getIMMDevice( const std::string &key );

  private:

	  // TODO: this is suitable for the base class and public API
	  // - but need to consider both how to get device by unique key and human-readable name
	  // - also need to navigate msw liking wstring and mac string
	  DeviceRef getDevice( const std::string &key );

	  struct DeviceInfo {
		  std::string key;
		  std::string name;
		  enum Usage { Input, Output } usage; // TODO: add field for I/O
		  std::wstring			deviceId;

		  DeviceRef			device;
		  size_t numChannels, sampleRate;
	  };

	  DeviceInfo& getDeviceInfo( const std::string &key );
	  void parseDevices( DeviceInfo::Usage usage );

	  typedef std::vector<DeviceInfo> DeviceContainerT;
	  DeviceContainerT& getDevices();

	  DeviceContainerT mDevices;
};

} // namespace audio2