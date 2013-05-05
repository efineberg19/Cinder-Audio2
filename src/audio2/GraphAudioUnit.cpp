#include "audio2/GraphAudioUnit.h"
#include "audio2/DeviceAudioUnit.h"
#include "audio2/cocoa/Util.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

using namespace std;

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - OutputAudioUnit
// ----------------------------------------------------------------------------------------------------

	OutputAudioUnit::OutputAudioUnit( DeviceRef device )
	: Output( device )
	{
		mTag = "OutputAudioUnit";
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
		CI_ASSERT( status == noErr );

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

	// TODO: should use kAudioUnitProperty_LastRenderError from outside renderloop, or if that doesn't work well use an atomic<OSStatus> that one can check from UI loop
	// FIXME: render is going in the wrong order - needs to be depth first
	OSStatus OutputAudioUnit::renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList )
	{
		OutputAudioUnit *self = static_cast<OutputAudioUnit *>( context );
		if( self->mSources.empty() )
			return noErr;

		BufferT& buffer = self->mBuffer;
		CI_ASSERT( bufferList->mNumberBuffers == buffer.size() );
		CI_ASSERT( numFrames == buffer[0].size() ); // assumes non-interleaved

		self->renderNode( self->mSources[0], &buffer, flags, timeStamp, busNumber, numFrames, bufferList );
		
//		NodeRef node = self->mSources[0];
//		while( node ) {
//			if( node->getFormat().isNative() ) {
//				::AudioUnit audioUnit = static_cast<::AudioUnit>( node->getNative() );
//				OSStatus status = ::AudioUnitRender( audioUnit, flags, timeStamp, AudioUnitBus::Input, numFrames, bufferList );
//				CI_ASSERT( status == noErr );
//			} else {
//				node->render( &buffer );
//
//				// ???: how can I avoid this when generic nodes are chained together?
//				// TODO: I can probably just memcpy all of mBuffer right over, but taking the safe route for now
//				for( UInt32 i = 0; i < bufferList->mNumberBuffers; i++ ) {
//					memcpy( bufferList->mBuffers[i].mData, buffer[i].data(), bufferList->mBuffers[i].mDataByteSize );
//				}
//			}
//
//			if( node->getSources().empty() )
//				break;
//			node = node->getSources().front();
//		}

		return noErr;
	}

	OSStatus OutputAudioUnit::renderNode( NodeRef node, BufferT *buffer, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *auBufferList )
	{
		if( ! node )
			return noErr; // ???: silence?

		// render children first
		if( ! node->getSources().empty() )
			renderNode( node->getSources().front(), buffer, flags, timeStamp, busNumber, numFrames, auBufferList );

		if( node->getFormat().isNative() ) {
			::AudioUnit audioUnit = static_cast<::AudioUnit>( node->getNative() );
			OSStatus status = ::AudioUnitRender( audioUnit, flags, timeStamp, busNumber, numFrames, auBufferList );
			CI_ASSERT( status == noErr ); // FIXME: kAudioUnitErr_NoConnection - does it have to be connected to Output unit?
		} else {
			node->render( buffer );

			// ???: how can I avoid this when generic nodes are chained together?
			// TODO: I can probably just memcpy all of mBuffer right over, but taking the safe route for now
			for( UInt32 i = 0; i < auBufferList->mNumberBuffers; i++ ) {
				memcpy( auBufferList->mBuffers[i].mData, buffer->at( i ).data(), auBufferList->mBuffers[i].mDataByteSize );
			}
		}

		return noErr;
	}

// ----------------------------------------------------------------------------------------------------
// MARK: - ProcessorAudioUnit
// ----------------------------------------------------------------------------------------------------

	// (from AudioUnitParameters.h)

// Parameters for the AULowpass unit
//	enum {
//		// Global, Hz, 10->(SampleRate/2), 6900
//		kLowPassParam_CutoffFrequency 			= 0,
//
//		// Global, dB, -20->40, 0
//		kLowPassParam_Resonance 				= 1
//	};

	ProcessorAudioUnit::ProcessorAudioUnit()
	{
		mTag = "ProcessorAudioUnit";
		mFormat.mIsNative = true;
		
		mEffectSubType = kAudioUnitSubType_LowPassFilter; // testing
	}

	void ProcessorAudioUnit::initialize()
	{
		AudioComponentDescription comp{ 0 };
		comp.componentType = kAudioUnitType_Effect;
		comp.componentSubType = mEffectSubType;
		comp.componentManufacturer = kAudioUnitManufacturer_Apple;


		cocoa::findAndCreateAudioComponent( comp,  &mAudioUnit );

		auto source = mSources.front();
		CI_ASSERT( source );

//		mFormat.mNumChannels = source->getFormat().getNumChannels();
//		mFormat.mSampleRate = source->getFormat().getSampleRate();
		mFormat.mNumChannels = 2;
		mFormat.mSampleRate = 44100;
		::AudioStreamBasicDescription asbd = cocoa::nonInterleavedFloatABSD( mFormat.mNumChannels, mFormat.mSampleRate );

		OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, AudioUnitBus::Output, &asbd, sizeof( asbd ) );
		CI_ASSERT( status == noErr );


		status = ::AudioUnitInitialize( mAudioUnit );
		CI_ASSERT( status == noErr );
	}

	void ProcessorAudioUnit::setParameter( ::AudioUnitParameterID param, float val )
	{
		OSStatus status = ::AudioUnitSetParameter( mAudioUnit, param, kAudioUnitScope_Global, 0, val, 0 );
		CI_ASSERT( status == noErr );
	}

} // namespace audio2