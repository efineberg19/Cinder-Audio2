#include "audio2/Device.h"
#include "audio2/DeviceAudioUnit.h"
#include "audio2/Debug.h"

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - OutputDevice
// ----------------------------------------------------------------------------------------------------

OutputDeviceRef OutputDevice::getDefault()
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
#if defined( CINDER_COCOA )
		sInstance = new DeviceManagerAudioUnit;
#else
		throw "not implemented";
#endif
	}
	return sInstance;
}



} // namespace audio2