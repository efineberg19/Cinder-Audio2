#pragma once

#include "audio2/Graph.h"
#include "audio2/Device.h"
#include "audio2/GeneratorNode.h"

namespace audio2 {

class Engine {
  public:
	virtual ~Engine() {}

	virtual GraphRef			createGraph() = 0;
	virtual MixerNodeRef		createMixer() = 0;
	virtual OutputNodeRef		createOutput( DeviceRef device = Device::getDefaultOutput() ) = 0;
	virtual InputNodeRef		createInput( DeviceRef device = Device::getDefaultInput() ) = 0;

	static Engine* instance();
};

} // namespace audio2