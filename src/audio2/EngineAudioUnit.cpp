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

} // namespace audio2