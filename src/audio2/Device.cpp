#include "audio2/Device.h"
#include "audio2/audio.h"
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

const std::string& Device::getName()
{
	if( mName.empty() )
		mName = DeviceManager::instance()->getName( mKey );

	return mName;
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

size_t Device::getBlockSize()
{
	return DeviceManager::instance()->getBlockSize( mKey );
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