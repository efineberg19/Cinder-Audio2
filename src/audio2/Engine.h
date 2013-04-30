#pragma once

#include "audio2/Graph.h"
#include "audio2/Device.h"

namespace audio2 {

	class Engine {
	public:
		virtual ~Engine() {}

		virtual OutputRef createOutputSpeakers( DeviceRef device ) = 0;

		static Engine* instance();
	};
	

} // namespace audio2