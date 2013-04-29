#include "audio2/Device.h"
#include "audio2/Debug.h"

#if defined( CINDER_COCOA )
	#include "audio2/DeviceAudioUnit.h"
	#if defined( CINDER_MAC )
		#include "audio2/DeviceManagerCoreAudio.h"
	#else
		#include "audio2/DeviceManagerAudioSession.h"
	#endif
#endif

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - OutputDevice
// ----------------------------------------------------------------------------------------------------

DeviceRef Device::getDefaultOutput()
{
	return DeviceManager::instance()->getDefaultOutput();
}

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManager
// ----------------------------------------------------------------------------------------------------

DeviceManager* DeviceManager::instance()
{
	static DeviceManager *sInstance = 0;
	if( ! sInstance ) {
#if defined( CINDER_MAC )
		sInstance = new DeviceManagerCoreAudio();
#elif defined( CINDER_COCOA_TOUCH )
		sInstance = new DeviceManagerAudioSession();
#else
	#error "not implemented"
#endif
	}
	return sInstance;
}



} // namespace audio2