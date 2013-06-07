#pragma once

#include "audio2/Engine.h"
#include "audio2/GeneratorNode.h"

namespace audio2 {

class EngineXAudio : public Engine {
  public:
	virtual GraphRef		createGraph() override;
	virtual MixerNodeRef	createMixer() override;
	virtual OutputNodeRef	createOutput( DeviceRef device ) override;
	virtual InputNodeRef	createInput( DeviceRef device ) override;
};

} // namespace audio2