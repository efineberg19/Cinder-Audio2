#include "audio2/EngineAudioUnit.h"
#include "audio2/GraphAudioUnit.h"

using namespace std;

namespace audio2 {

	OutputRef EngineAudioUnit::createOutputSpeakers( DeviceRef device )
	{
		return OutputRef( new SpeakerOutputAudioUnit( device ) );
	}

} // namespace audio2