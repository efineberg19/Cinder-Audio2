#include "audio2/EngineXAudio.h"
#include "audio2/GraphXAudio.h"
#include "audio2/DeviceInputWasapi.h"
#include "audio2/assert.h"

using namespace std;

namespace audio2 {

GraphRef EngineXAudio::createGraph()
{
	return GraphRef( new GraphXAudio() );
}

ConsumerRef EngineXAudio::createOutput( DeviceRef device )
{
	return ConsumerRef( new OutputXAudio( device ) );
}

ProducerRef EngineXAudio::createInput( DeviceRef device )
{
	return ProducerRef( new InputWasapi( device ) );
}

MixerRef EngineXAudio::createMixer()
{
	return MixerRef( new MixerXAudio() );
}

} // namespace audio2