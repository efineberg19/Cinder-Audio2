#pragma once

#include "audio2/Device.h"

#include <AudioUnit/AudioUnit.h>
#include <string>

namespace audio2 {

struct DeviceAudioUnitImpl;

class DeviceAudioUnit : public Device {
  public:
	virtual ~DeviceAudioUnit();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

  private:
	DeviceAudioUnit( const ::AudioComponentDescription &component, const std::string &key );
	std::unique_ptr<DeviceAudioUnitImpl> mImpl;
	std::string mKey;

	 // ???: friend DeviceManagers here or can be avoided?
#if defined( CINDER_MAC )
	friend class DeviceManagerCoreAudio;
#elif defined( CINDER_COCOA_TOUCH )
	friend class DeviceManagerAudioSession;
#endif
};

} // namespace audio2
