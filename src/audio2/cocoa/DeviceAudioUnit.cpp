#include "audio2/cocoa/DeviceAudioUnit.h"
#include "audio2/cocoa/CinderCoreAudio.h"
#include "audio2/audio.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

using namespace std;
using namespace ci;

namespace audio2 { namespace cocoa {

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceAudioUnit
// ----------------------------------------------------------------------------------------------------

DeviceAudioUnit::DeviceAudioUnit( const std::string &key, const ::AudioComponentDescription &component )
: Device( key ), mComponentDescription( component ), mComponentInstance( NULL ), mInputConnected( false ), mOutputConnected( false )
{
}

DeviceAudioUnit::~DeviceAudioUnit()
{
}

void DeviceAudioUnit::initialize()
{
	if( mInitialized ) {
		LOG_E << "already initialized." << endl;
		return;
	}

	UInt32 enableInput = static_cast<UInt32>( mInputConnected );
	::AudioUnitSetProperty( getComponentInstance(), kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, Bus::Input, &enableInput, sizeof( enableInput ) );
	LOG_V << "input enabled: " << enableInput << endl;

	UInt32 enableOutput = static_cast<UInt32>( mOutputConnected );
	::AudioUnitSetProperty( getComponentInstance(), kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, Bus::Output, &enableOutput, sizeof( enableOutput ) );
	LOG_V << "output enabled: " << enableOutput << endl;

	DeviceManager::instance()->setActiveDevice( mKey );

	OSStatus status = ::AudioUnitInitialize( getComponentInstance() );
	CI_ASSERT( status == noErr );

	LOG_V << "success initializing device: " << getName() << endl;
	mInitialized = true;
}

void DeviceAudioUnit::uninitialize()
{
	if( ! mInitialized ) {
		LOG_E << "not initialized." << endl;
		return;
	}

	LOG_V << "unitinializing device: " << getName() << endl;

	if( mComponentInstance ) {
		OSStatus status = ::AudioUnitUninitialize( mComponentInstance );
		CI_ASSERT( status == noErr );
		status = ::AudioComponentInstanceDispose( mComponentInstance );
		CI_ASSERT( status == noErr );

		mComponentInstance = NULL;
	}
	mInitialized = false;
}

void DeviceAudioUnit::start()
{
	if( ! mInitialized || mEnabled ) {
		LOG_E << boolalpha << "(returning) mInitialized: " << mInitialized << ", mEnabled: " << mEnabled << dec << endl;
		return;
	}

	mEnabled = true;
	OSStatus status = ::AudioOutputUnitStart( mComponentInstance );
	CI_ASSERT( status == noErr );
}

void DeviceAudioUnit::stop()
{
	if( ! mInitialized || ! mEnabled ) {
		LOG_E << boolalpha << "(returning) mInitialized: " << mInitialized << ", mEnabled: " << mEnabled << dec << endl;
		return;
	}

	mEnabled = false;
	OSStatus status = ::AudioOutputUnitStop( mComponentInstance );
	CI_ASSERT( status == noErr );
}

const ::AudioComponentInstance& DeviceAudioUnit::getComponentInstance()
{
	if( ! mComponentInstance ) {
		LOG_V << "creating component instance." << endl;
		cocoa::findAndCreateAudioComponent( mComponentDescription, &mComponentInstance );
	}
	return mComponentInstance;
}

} } // namespace audio2::cocoa