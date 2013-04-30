#include "audio2/Node.h"

namespace audio2 {

	OutputRef SpeakerOutput::create( DeviceRef device )
	{
		// TODO NEXT: need an impl specific Output, such as SpeakerOutputAudioUnit
		// - or do I? Can I just tell it to connect to me?
		// - I do, I need a render callback
		return OutputRef();
	}

} // namespace audio2