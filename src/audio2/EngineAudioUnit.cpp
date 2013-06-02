#include "audio2/EngineAudioUnit.h"
#include "audio2/GraphAudioUnit.h"

using namespace std;

namespace audio2 {

GraphRef EngineAudioUnit::createGraph()
{
	return GraphRef( new GraphAudioUnit() );
}

OutputNodeRef EngineAudioUnit::createOutput( DeviceRef device )
{
	return OutputNodeRef( new OutputAudioUnit( device ) );
}

InputNodeRef EngineAudioUnit::createInput( DeviceRef device )
{
	return InputNodeRef( new InputAudioUnit( device ) );
}

MixerNodeRef EngineAudioUnit::createMixer()
{
	return MixerNodeRef( new MixerAudioUnit() );
}

} // namespace audio2