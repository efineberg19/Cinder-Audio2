#include "audio2/Device.h"
#include "audio2/audio.h"
#include "audio2/Debug.h"

#if defined( CINDER_COCOA )
	#include "audio2/cocoa/DeviceAudioUnit.h"
	#if defined( CINDER_MAC )
		#include "audio2/cocoa/DeviceManagerCoreAudio.h"
	#else
		#include "audio2/cocoa/DeviceManagerAudioSession.h"
	#endif
#elif defined( CINDER_MSW )
	#include "audio2/msw/DeviceManagerWasapi.h"
#endif

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - Device
// ----------------------------------------------------------------------------------------------------

DeviceRef Device::getDefaultOutput()
{
	return DeviceManager::instance()->getDefaultOutput();
}

DeviceRef Device::getDefaultInput()
{
	return DeviceManager::instance()->getDefaultInput();
}

const std::vector<DeviceRef>& Device::getDevices()
{
	return DeviceManager::instance()->getDevices();
}

const std::string& Device::getName()
{
	if( mName.empty() )
		mName = DeviceManager::instance()->getName( mKey );

	return mName;
}

const std::string& Device::getKey()
{
	return mKey;
}

size_t Device::getNumInputChannels()
{
	return DeviceManager::instance()->getNumInputChannels( mKey );
}

size_t Device::getNumOutputChannels()
{
	return DeviceManager::instance()->getNumOutputChannels( mKey );
}

size_t Device::getSampleRate()
{
	return DeviceManager::instance()->getSampleRate( mKey );
}

size_t Device::getNumFramesPerBlock()
{
	return DeviceManager::instance()->getNumFramesPerBlock( mKey );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManager
// ----------------------------------------------------------------------------------------------------

DeviceManager* DeviceManager::instance()
{
	static DeviceManager *sInstance = 0;
	if( ! sInstance ) {
#if defined( CINDER_MAC )
		sInstance = new cocoa::DeviceManagerCoreAudio();
#elif defined( CINDER_COCOA_TOUCH )
		sInstance = new cocoa::DeviceManagerAudioSession();
#elif defined( CINDER_MSW )
		sInstance = new msw::DeviceManagerWasapi();
#endif
	}
	return sInstance;
}

} // namespace audio2