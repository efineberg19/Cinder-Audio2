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
	virtual RootNodeRef			createOutput( DeviceRef device ) = 0;
	virtual GeneratorNodeRef	createInput( DeviceRef device ) = 0;

	static Engine* instance();
};

} // namespace audio2