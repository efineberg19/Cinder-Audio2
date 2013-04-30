#pragma once

#include "audio2/Graph.h"

namespace audio2 {

	class DeviceAudioUnit;
	
	class SpeakerOutputAudioUnit : public SpeakerOutput {
	public:
		SpeakerOutputAudioUnit( DeviceRef device );
	private:
		std::shared_ptr<DeviceAudioUnit> mDevice;
	};

	class GraphAudioUnit : public Graph {

	};

} // namespace audio2