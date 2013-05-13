#include "audio2/EngineAudioUnit.h"
#include "audio2/GraphAudioUnit.h"

using namespace std;

namespace audio2 {

	GraphRef EngineAudioUnit::createGraph()
	{
		return GraphRef( new GraphAudioUnit() );
	}

	ConsumerRef EngineAudioUnit::createOutput( DeviceRef device )
	{
		return ConsumerRef( new OutputAudioUnit( device ) );
	}

	ProducerRef EngineAudioUnit::createInput( DeviceRef device )
	{
		return ProducerRef( new InputAudioUnit( device ) );
	}

	MixerRef EngineAudioUnit::createMixer()
	{
		return MixerRef( new MixerAudioUnit() );
	}

} // namespace audio2