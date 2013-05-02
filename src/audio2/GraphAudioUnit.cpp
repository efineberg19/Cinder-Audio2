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

		// TODO: set format params in graph, expose for customization
//		CI_ASSERT( mFormat.isComplete() );
		mFormat.mNumChannels = mDevice->getNumOutputChannels();
		mFormat.mSampleRate = mDevice->getSampleRate();
		mASBD = cocoa::nonInterleavedFloatABSD( mFormat.mNumChannels, mFormat.mSampleRate );

		size_t blockSize = mDevice->getBlockSize();
		mBuffer.resize( mFormat.mNumChannels );
		for( auto& channel : mBuffer )
			channel.resize( blockSize );

		LOG_V << "resized buffer to " << mFormat.mNumChannels << " channels, " << blockSize << " samples per block" << endl;

		::AURenderCallbackStruct callbackStruct;
		callbackStruct.inputProc = OutputAudioUnit::renderCallback;
		callbackStruct.inputProcRefCon = this;

		OSStatus status = ::AudioUnitSetProperty( mDevice->getComponentInstance(), kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, AudioUnitBus::Output, &callbackStruct, sizeof(callbackStruct) );
		CI_ASSERT( status == noErr ); // FIXME: -50 here on iOS (sim only?).

		status = ::AudioUnitSetProperty( mDevice->getComponentInstance(), kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, AudioUnitBus::Output, &mASBD, sizeof(mASBD) );
		CI_ASSERT( status == noErr );
		
		mDevice->setOutputConnected();
		LOG_V << "output connected." << endl;

		mDevice->initialize();
	}

	void OutputAudioUnit::uninitialize()
	{
		mDevice->uninitialize();
	}

	void OutputAudioUnit::start()
	{
		mDevice->start();
		LOG_V << "started: " << mDevice->getName() << endl;
	}

	void OutputAudioUnit::stop()
	{
		mDevice->stop();
		LOG_V << "stopped: " << mDevice->getName() << endl;
	}

	// TODO: here, I need to navigate both native and non-native units.
	//	- on native ones, I can call AudioUnitRender
	//  - generic, use mBuffer
	// Do this in default render?
	OSStatus OutputAudioUnit::renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList )
	{
		OutputAudioUnit *self = static_cast<OutputAudioUnit *>( context );
		if( self->mSources.empty() )
			return noErr;

		BufferT& buffer = self->mBuffer;
		CI_ASSERT( bufferList->mNumberBuffers == buffer.size() );
		CI_ASSERT( numFrames == buffer[0].size() ); // assumes non-interleaved

		self->mSources[0]->render( &buffer );

		// TODO: I can probably just memcpy all of mBuffer right over, but taking the safe route for now
		for( UInt32 i = 0; i < bufferList->mNumberBuffers; i++ ) {
			memcpy( bufferList->mBuffers[i].mData, buffer[i].data(), bufferList->mBuffers[i].mDataByteSize );
		}

		return noErr;
	}

} // namespace audio2