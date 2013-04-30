#pragma once

#include "audio2/Device.h"

#include <AudioUnit/AudioUnit.h>

namespace audio2 {

enum AudioUnitBus {
	Output	= 0,
	Input	= 1
};

class DeviceAudioUnit : public Device {
  public:
	virtual ~DeviceAudioUnit();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	const std::string& getName() override;
	size_t getNumInputChannels() override;
	size_t getNumOutputChannels() override;
	size_t getSampleRate() override;
	size_t getBlockSize() override;

  private:
	DeviceAudioUnit( const ::AudioComponentDescription &component, const std::string &key );

//	struct DeviceAudioUnitImpl;
//	std::unique_ptr<DeviceAudioUnitImpl> mImpl;

	::AudioComponentDescription mComponentDescription;
	::AudioComponentInstance	mComponentInstance;

	std::string mKey, mName;

	// TODO NEXT: in's and out's need to be connected before initialize is called
	bool mInputConnected, mOutputConnected;
	
	 // ???: friend DeviceManagers here or can be avoided?
#if defined( CINDER_MAC )
	friend class DeviceManagerCoreAudio;
#elif defined( CINDER_COCOA_TOUCH )
	friend class DeviceManagerAudioSession;
#endif
};

} // namespace audio2
