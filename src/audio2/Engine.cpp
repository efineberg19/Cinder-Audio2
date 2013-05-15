#include "audio2/Engine.h"

#include "cinder/Cinder.h"

#if defined( CINDER_COCOA )
	#include "audio2/EngineAudioUnit.h"
#else
	#error "not implemented"
#endif

namespace audio2 {

Engine* Engine::instance()
{
	static Engine *sInstance = 0;
	if( ! sInstance ) {
#if defined( CINDER_COCOA )
		sInstance = new EngineAudioUnit();
#else
#endif
	}
	return sInstance;
}

} // namespace audio2