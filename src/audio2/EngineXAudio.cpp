#include "audio2/EngineXAudio.h"
#include "audio2/GraphXAudio.h"

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
	CI_ASSERT( 0 && "not yet implemtned" );
	//return ProducerRef( new InputAudioUnit( device ) );
	return ProducerRef();
}

MixerRef EngineXAudio::createMixer()
{
	CI_ASSERT( 0 && "not yet implemtned" );
	//return MixerRef( new MixerXAudio() );
	return MixerRef();
}

} // namespace audio2