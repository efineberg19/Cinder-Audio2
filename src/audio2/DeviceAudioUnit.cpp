#include "audio2/DeviceAudioUnit.h"
#include "audio2/audio.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"
#include "audio2/cocoa/Util.h"

using namespace std;
using namespace ci;

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceAudioUnitImpl Declaration
// ----------------------------------------------------------------------------------------------------

//struct DeviceAudioUnitImpl {
//	DeviceAudioUnitImpl()	{}
//
//};

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceAudioUnit
// ----------------------------------------------------------------------------------------------------

DeviceAudioUnit::DeviceAudioUnit( const ::AudioComponentDescription &component, const std::string &key )
: Device(), mComponentDescription( component ), mComponentInstance( NULL ), mKey( key ), mInputConnected( false ), mOutputConnected( false )
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

	cocoa::findAndCreateAudioComponent( mComponentDescription, &mComponentInstance );

	UInt32 enableInput = static_cast<UInt32>( mInputConnected );
	AudioUnitSetProperty( mComponentInstance, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, AudioUnitBus::Input, &enableInput, sizeof( enableInput ) );
	LOG_V << "input enabled: " << enableInput << endl;

	UInt32 enableOutput = static_cast<UInt32>( mOutputConnected );
	AudioUnitSetProperty( mComponentInstance, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, AudioUnitBus::Output, &enableOutput, sizeof( enableOutput ) );
	LOG_V << "output enabled: " << enableOutput << endl;

	DeviceManager::instance()->setActiveDevice( mKey );

	OSStatus status = AudioUnitInitialize( mComponentInstance );
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
		mComponentInstance = NULL;
	}
	mInitialized = false;
}

void DeviceAudioUnit::start()
{
	if( ! mInitialized || mRunning ) {
		LOG_E << "(returning) mInitialized: " << mInitialized << ", mRunning: " << endl;
		return;
	}

	mRunning = true;
	OSStatus status = AudioOutputUnitStart( mComponentInstance );
	CI_ASSERT( status == noErr );
}

void DeviceAudioUnit::stop()
{
	if( ! mInitialized || ! mRunning ) {
		LOG_E << "(returning) mInitialized: " << mInitialized << ", mRunning: " << endl;
		return;
	}

	mRunning = false;
	OSStatus status = AudioOutputUnitStop( mComponentInstance );
	CI_ASSERT( status == noErr );
}

const std::string& DeviceAudioUnit::getName()
{
	mName =  string( "TODO" );

	return mName;
}

size_t DeviceAudioUnit::getNumInputChannels()
{
	throw "not implemtned";
	return 0;
}

size_t DeviceAudioUnit::getNumOutputChannels()
{
	throw "not implemtned";
	return 0;
}

size_t DeviceAudioUnit::getSampleRate()
{
	throw "not implemtned";
	return 0;
}

size_t DeviceAudioUnit::getBlockSize()
{
	throw "not implemtned";
	return 0;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceAudioUnitImpl
// ----------------------------------------------------------------------------------------------------



} // namespace audio2
