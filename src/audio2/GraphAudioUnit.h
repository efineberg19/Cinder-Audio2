#pragma once

#include "audio2/Graph.h"

#include <AudioUnit/AudioUnit.h>

namespace audio2 {

	class DeviceAudioUnit;

	
	class SpeakerOutputAudioUnit : public SpeakerOutput {
	public:
		SpeakerOutputAudioUnit( DeviceRef device );

		void initialize() override;
		void uninitialize() override;

		void start() override;
		void stop() override;

	private:
		static OSStatus renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *data );

		std::shared_ptr<DeviceAudioUnit> mDevice;
		::AudioStreamBasicDescription mASBD;
	};

	class GraphAudioUnit : public Graph {

	};

} // namespace audio2