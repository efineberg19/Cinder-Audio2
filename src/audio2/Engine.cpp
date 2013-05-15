#include "audio2/Engine.h"

#include "cinder/Cinder.h"

#if defined( CINDER_COCOA )
	#include "audio2/EngineAudioUnit.h"
#elif defined( CINDER_MSW )
	#include "audio2/EngineXAudio.h"
#endif

namespace audio2 {

Engine* Engine::instance()
{
	static Engine *sInstance = 0;
	if( ! sInstance ) {
#if defined( CINDER_COCOA )
		sInstance = new EngineAudioUnit();
#elif defined( CINDER_MSW )
		sInstance = new EngineXAudio();
#else
		// TODO: add hook here to get user defined engine impl
#error "not implemented."
#endif
	}
	return sInstance;
}

} // namespace audio2