#pragma once

#include "audio2/Engine.h"

namespace audio2 {

class EngineXAudio : public Engine {
  public:
	virtual GraphRef	createGraph() override;
	virtual MixerNodeRef	createMixer() override;
	virtual RootNodeRef createOutput( DeviceRef device ) override;
	virtual GeneratorNodeRef	createInput( DeviceRef device ) override;
};

} // namespace audio2