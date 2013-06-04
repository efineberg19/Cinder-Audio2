#include "audio2/DeviceAudioUnit.h"
#include "audio2/audio.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"
#include "audio2/cocoa/Util.h"

using namespace std;
using namespace ci;

// TODO: place all AudioUnit funciton calls in global namespace

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceAudioUnit
// ----------------------------------------------------------------------------------------------------

DeviceAudioUnit::DeviceAudioUnit( const ::AudioComponentDescription &component, const std::string &key )
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
	AudioUnitSetProperty( getComponentInstance(), kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, Bus::Input, &enableInput, sizeof( enableInput ) );
	LOG_V << "input enabled: " << enableInput << endl;

	UInt32 enableOutput = static_cast<UInt32>( mOutputConnected );
	AudioUnitSetProperty( getComponentInstance(), kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, Bus::Output, &enableOutput, sizeof( enableOutput ) );
	LOG_V << "output enabled: " << enableOutput << endl;

	DeviceManager::instance()->setActiveDevice( mKey );

	OSStatus status = AudioUnitInitialize( getComponentInstance() );
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
		OSStatus status = AudioUnitUninitialize( mComponentInstance );
		CI_ASSERT( status == noErr );
		status = AudioComponentInstanceDispose( mComponentInstance );
		CI_ASSERT( status == noErr );

		mComponentInstance = NULL;
	}
	mInitialized = false;
}

void DeviceAudioUnit::start()
{
	if( ! mInitialized || mRunning ) {
		LOG_E << boolalpha << "(returning) mInitialized: " << mInitialized << ", mRunning: " << mRunning << dec << endl;
		return;
	}

	mRunning = true;
	OSStatus status = AudioOutputUnitStart( mComponentInstance );
	CI_ASSERT( status == noErr );
}

void DeviceAudioUnit::stop()
{
	if( ! mInitialized || ! mRunning ) {
		LOG_E << boolalpha << "(returning) mInitialized: " << mInitialized << ", mRunning: " << mRunning << dec << endl;
		return;
	}

	mRunning = false;
	OSStatus status = AudioOutputUnitStop( mComponentInstance );
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

} // namespace audio2
