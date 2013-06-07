#include "audio2/EngineXAudio.h"
#include "audio2/GraphXAudio.h"
#include "audio2/DeviceInputWasapi.h"
#include "audio2/assert.h"

// FIXME: Need to have one place that determines if Wasapi is available (ala #if( _WIN32_WINNT < 0x600 ))
// - if it isn't, don't include DeviceInputWasapi.h and createInput returns null
// - #define CINDER_AUDIO_NO_INPUT ?

using namespace std;

namespace audio2 {

GraphRef EngineXAudio::createGraph()
{
	return GraphRef( new GraphXAudio() );
}

OutputNodeRef EngineXAudio::createOutput( DeviceRef device )
{
	return OutputNodeRef( new OutputXAudio( device ) );
}

InputNodeRef EngineXAudio::createInput( DeviceRef device )
{
	return InputNodeRef( new InputWasapi( device ) );
}

MixerNodeRef EngineXAudio::createMixer()
{
	return MixerNodeRef( new MixerXAudio() );
}

} // namespace audio2