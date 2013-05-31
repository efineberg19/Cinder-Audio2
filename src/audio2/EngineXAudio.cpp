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

RootNodeRef EngineXAudio::createOutput( DeviceRef device )
{
	return RootNodeRef( new OutputXAudio( device ) );
}

GeneratorNodeRef EngineXAudio::createInput( DeviceRef device )
{
	return GeneratorNodeRef( new InputWasapi( device ) );
}

MixerNodeRef EngineXAudio::createMixer()
{
	return MixerNodeRef( new MixerXAudio() );
}

} // namespace audio2