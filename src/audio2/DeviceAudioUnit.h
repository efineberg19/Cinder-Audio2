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

	// TODO: I'm still deciding whether to hide this and attach all render callbacks within this class.
	const ::AudioComponentInstance& getComponentInstance();
	bool isInputConnected() const	{ return mInputConnected; }
	bool isOutputConnected() const	{ return mOutputConnected; }
	void setInputConnected()	{ mInputConnected = true; }
	void setOutputConnected()	{ mOutputConnected = true; }

  private:
	DeviceAudioUnit( const ::AudioComponentDescription &component, const std::string &key );

//	struct DeviceAudioUnitImpl;
//	std::unique_ptr<DeviceAudioUnitImpl> mImpl;

	::AudioComponentDescription mComponentDescription;
	::AudioComponentInstance	mComponentInstance;

	bool mInputConnected, mOutputConnected;
	
	 // ???: friend DeviceManagers here or can be avoided?
#if defined( CINDER_MAC )
	friend class DeviceManagerCoreAudio;
#elif defined( CINDER_COCOA_TOUCH )
	friend class DeviceManagerAudioSession;
#endif
};

} // namespace audio2
