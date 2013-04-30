#pragma once

#include "audio2/audio.h"
#include "audio2/Device.h"

namespace audio2 {

// ???: alt name: DeviceManagerCocoaTouch
class DeviceManagerAudioSession : public DeviceManager {
  public:
	DeviceRef getDefaultOutput() override;
	DeviceRef getDefaultInput() override;
  private:

	DeviceRef getRemoteIOUnit();
	DeviceRef mRemoteIOUnit;
};

} // audio2