#pragma once

#include "audio2/Engine.h"

namespace audio2 {

	class EngineAudioUnit : public Engine {

		virtual OutputRef createOutputSpeakers( DeviceRef device ) override;

	};
	
} // namespace audio2