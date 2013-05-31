#pragma once

#include "audio2/Engine.h"

namespace audio2 {

class EngineXAudio : public Engine {
  public:
	virtual GraphRef	createGraph() override;
	virtual MixerRef	createMixer() override;
	virtual RootRef createOutput( DeviceRef device ) override;
	virtual GeneratorRef	createInput( DeviceRef device ) override;
};

} // namespace audio2