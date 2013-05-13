#pragma once

#include "audio2/Graph.h"
#include "audio2/Device.h"

namespace audio2 {

	class Engine {
	public:
		virtual ~Engine() {}

		virtual GraphRef	createGraph() = 0;
		virtual MixerRef	createMixer() = 0;
		virtual ConsumerRef createOutput( DeviceRef device ) = 0;
		virtual ProducerRef createInput( DeviceRef device ) = 0;

		static Engine* instance();
	};
	

} // namespace audio2