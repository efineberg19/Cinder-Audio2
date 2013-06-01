#include "audio2/EngineAudioUnit.h"
#include "audio2/GraphAudioUnit.h"

using namespace std;

namespace audio2 {

GraphRef EngineAudioUnit::createGraph()
{
	return GraphRef( new GraphAudioUnit() );
}

RootNodeRef EngineAudioUnit::createOutput( DeviceRef device )
{
	return RootNodeRef( new OutputAudioUnit( device ) );
}

GeneratorNodeRef EngineAudioUnit::createInput( DeviceRef device )
{
	return GeneratorNodeRef( new InputAudioUnit( device ) );
}

MixerNodeRef EngineAudioUnit::createMixer()
{
	return MixerNodeRef( new MixerAudioUnit() );
}

} // namespace audio2