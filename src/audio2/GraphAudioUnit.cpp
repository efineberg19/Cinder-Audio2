#include "audio2/GraphAudioUnit.h"
#include "audio2/DeviceAudioUnit.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

using namespace std;

namespace audio2 {

	SpeakerOutputAudioUnit::SpeakerOutputAudioUnit( DeviceRef device )
	: SpeakerOutput( device )
	{
		mDevice = dynamic_pointer_cast<DeviceAudioUnit>( device );
		CI_ASSERT( mDevice );

		LOG_V << "done." << endl;
	}


} // namespace audio2