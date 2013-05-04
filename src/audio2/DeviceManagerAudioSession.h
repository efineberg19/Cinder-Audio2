#pragma once

#include "audio2/audio.h"
#include "audio2/Device.h"

namespace audio2 {

// ???: alt name: DeviceManagerCocoaTouch
class DeviceManagerAudioSession : public DeviceManager {
  public:
	DeviceManagerAudioSession();
	virtual ~DeviceManagerAudioSession() = default;

	DeviceRef getDefaultOutput() override;
	DeviceRef getDefaultInput() override;

	std::string getName( const std::string &key ) override;
	size_t getNumInputChannels( const std::string &key ) override;
	size_t getNumOutputChannels( const std::string &key ) override;
	size_t getSampleRate( const std::string &key ) override;
	size_t getBlockSize( const std::string &key ) override;

	void setActiveDevice( const std::string &key ) override;

	bool inputIsEnabled();

  private:

	DeviceRef	getRemoteIOUnit();
	void		activateSession();
	UInt32		getSessionCategory(); // ???: map to my own, type-safe enum? Currently I'm just using the raw values in AudioSession.h


	DeviceRef mRemoteIOUnit;

	bool mSessionIsActive;
};

} // audio2