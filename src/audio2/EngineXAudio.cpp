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

RootRef EngineXAudio::createOutput( DeviceRef device )
{
	return RootRef( new OutputXAudio( device ) );
}

GeneratorRef EngineXAudio::createInput( DeviceRef device )
{
	return GeneratorRef( new InputWasapi( device ) );
}

MixerRef EngineXAudio::createMixer()
{
	return MixerRef( new MixerXAudio() );
}

} // namespace audio2