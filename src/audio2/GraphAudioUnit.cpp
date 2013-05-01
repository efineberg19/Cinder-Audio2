#include "audio2/GraphAudioUnit.h"
#include "audio2/DeviceAudioUnit.h"
#include "audio2/cocoa/Util.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

using namespace std;

namespace audio2 {

	OutputAudioUnit::OutputAudioUnit( DeviceRef device )
	: Output( device )
	{
		mDevice = dynamic_pointer_cast<DeviceAudioUnit>( device );
		CI_ASSERT( mDevice );

		LOG_V << "done." << endl;
	}

	void OutputAudioUnit::initialize()
	{
		CI_ASSERT( ! mDevice->isOutputConnected() );
		CI_ASSERT( mDevice->getComponentInstance() );
		
		// TODO: get format, check and configure ABSD based on that here

		mASBD = cocoa::nonInterleavedFloatABSD( mDevice->getNumInputChannels(), mDevice->getSampleRate() );

		::AURenderCallbackStruct callbackStruct;
		callbackStruct.inputProc = OutputAudioUnit::renderCallback;
		callbackStruct.inputProcRefCon = this;

		OSStatus status = ::AudioUnitSetProperty( mDevice->getComponentInstance(), kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, AudioUnitBus::Output, &callbackStruct, sizeof(callbackStruct) );
		CI_ASSERT( status == noErr ); // FIXME: -50 here.

		status = ::AudioUnitSetProperty( mDevice->getComponentInstance(), kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, AudioUnitBus::Output, &mASBD, sizeof(mASBD) );
		CI_ASSERT( status == noErr );
		
		mDevice->setOutputConnected();
		LOG_V << "output connected." << endl;
	}

	void OutputAudioUnit::uninitialize()
	{
	}

	void OutputAudioUnit::start()
	{
		mDevice->start();
		LOG_V << "started: " << mDevice->getName();
	}

	void OutputAudioUnit::stop()
	{
		mDevice->stop();
		LOG_V << "stopped: " << mDevice->getName();
	}

	OSStatus OutputAudioUnit::renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *data )
	{
		return noErr;
	}

} // namespace audio2