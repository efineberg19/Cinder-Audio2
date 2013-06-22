#pragma once

#include "audio2/audio.h"
#include "audio2/Device.h"

namespace audio2 { namespace cocoa {

class DeviceAudioUnit;

class DeviceManagerAudioSession : public DeviceManager {
  public:
	DeviceManagerAudioSession();
	virtual ~DeviceManagerAudioSession() = default;

	DeviceRef getDefaultOutput() override;
	DeviceRef getDefaultInput() override;
	DeviceRef findDeviceByName( const std::string &name ) override;
	DeviceRef findDeviceByKey( const std::string &key ) override;

	const std::vector<DeviceRef>& getDevices() override;

	std::string getName( const std::string &key ) override;
	size_t getNumInputChannels( const std::string &key ) override;
	size_t getNumOutputChannels( const std::string &key ) override;
	size_t getSampleRate( const std::string &key ) override;
	size_t getNumFramesPerBlock( const std::string &key ) override;

	void setActiveDevice( const std::string &key ) override;

	bool inputIsEnabled();

  private:

	std::shared_ptr<DeviceAudioUnit>	getRemoteIOUnit();
	void								activateSession();
	UInt32								getSessionCategory(); // TODO: consider useing the strings provided by AVAudioSession


	std::shared_ptr<DeviceAudioUnit> mRemoteIOUnit;

	bool mSessionIsActive;
};

} } // audio2::cocoa