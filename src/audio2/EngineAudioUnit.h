#pragma once

#include "audio2/Engine.h"

namespace audio2 {

	class EngineAudioUnit : public Engine {

		virtual GraphRef createGraph() override;
		virtual ConsumerRef createOutput( DeviceRef device ) override;

	};
	
} // namespace audio2