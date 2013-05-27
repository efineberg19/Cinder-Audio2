#pragma once

#include "audio2/Engine.h"

namespace audio2 {

class EngineXAudio : public Engine {
  public:
	virtual GraphRef createGraph() override;
	virtual MixerRef	createMixer() override;
	virtual ConsumerRef createOutput( DeviceRef device ) override;
	virtual ProducerRef createInput( DeviceRef device ) override;
};

} // namespace audio2