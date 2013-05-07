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
		mFormat.mIsNative = true;
		mDevice = dynamic_pointer_cast<DeviceAudioUnit>( device );
		CI_ASSERT( mDevice );
	}

	void OutputAudioUnit::initialize()
	{
		CI_ASSERT( ! mDevice->isOutputConnected() );

		::AudioUnit outputAudioUnit = static_cast<::AudioUnit>( mDevice->getComponentInstance() );
		CI_ASSERT( outputAudioUnit );

		// TODO: set format params in graph, expose for customization
//		CI_ASSERT( mFormat.isComplete() );
		mFormat.mNumChannels = mDevice->getNumOutputChannels();
		mFormat.mSampleRate = mDevice->getSampleRate();
		mASBD = cocoa::nonInterleavedFloatABSD( mFormat.mNumChannels, mFormat.mSampleRate );

		OSStatus status = ::AudioUnitSetProperty( outputAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, AudioUnitBus::Output, &mASBD, sizeof( mASBD ) );
		CI_ASSERT( status == noErr );

		mDevice->setOutputConnected();
		mDevice->initialize();

		LOG_V << "initialize complete." << endl;
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

	DeviceRef OutputAudioUnit::getDevice()
	{
		return std::static_pointer_cast<Device>( mDevice );
	}

	void* OutputAudioUnit::getNative()
	{
		return mDevice->getComponentInstance();
	}

	size_t OutputAudioUnit::getBlockSize() const
	{
		return mDevice->getBlockSize();
	}

// ----------------------------------------------------------------------------------------------------
// MARK: - EffectAudioUnit
// ----------------------------------------------------------------------------------------------------

	EffectAudioUnit::EffectAudioUnit(  UInt32 effectSubType )
	: mEffectSubType( effectSubType )
	{
		mTag = "EffectAudioUnit";
		mFormat.mIsNative = true;
	}

	void EffectAudioUnit::initialize()
	{
		AudioComponentDescription comp{ 0 };
		comp.componentType = kAudioUnitType_Effect;
		comp.componentSubType = mEffectSubType;
		comp.componentManufacturer = kAudioUnitManufacturer_Apple;


		cocoa::findAndCreateAudioComponent( comp, &mAudioUnit );

		auto source = mSources.front();
		CI_ASSERT( source );

		// TODO: some suggest AudioUnitGetProperty ( kAudioUnitProperty_StreamFormat ),  then set it's samplerate
//		mFormat.mNumChannels = source->getFormat().getNumChannels();
//		mFormat.mSampleRate = source->getFormat().getSampleRate();
		mFormat.mNumChannels = 2;
		mFormat.mSampleRate = 44100;
		::AudioStreamBasicDescription asbd = cocoa::nonInterleavedFloatABSD( mFormat.mNumChannels, mFormat.mSampleRate );

		OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, AudioUnitBus::Output, &asbd, sizeof( asbd ) );
		CI_ASSERT( status == noErr );

		status = ::AudioUnitInitialize( mAudioUnit );
		CI_ASSERT( status == noErr );

		LOG_V << "initialize complete. " << endl;
	}

	void EffectAudioUnit::setParameter( ::AudioUnitParameterID param, float val )
	{
		OSStatus status = ::AudioUnitSetParameter( mAudioUnit, param, kAudioUnitScope_Global, 0, val, 0 );
		CI_ASSERT( status == noErr );
	}

// ----------------------------------------------------------------------------------------------------
// MARK: - MixerAudioUnit
// ----------------------------------------------------------------------------------------------------

	MixerAudioUnit::MixerAudioUnit()
	{
		mTag = "MixerAudioUnit";
		mFormat.mIsNative = true;
	}

	void MixerAudioUnit::initialize()
	{
		AudioComponentDescription comp{ 0 };
		comp.componentType = kAudioUnitType_Mixer;
		comp.componentSubType = kAudioUnitSubType_MultiChannelMixer;
		comp.componentManufacturer = kAudioUnitManufacturer_Apple;

		cocoa::findAndCreateAudioComponent( comp, &mAudioUnit );

//		UInt32 busCount;
//		UInt32 busCountSize = sizeof( busCount );
//		OSStatus status = ::AudioUnitGetProperty( mAudioUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &busCount, &busCountSize );
//		CI_ASSERT( status == noErr );

		::AudioStreamBasicDescription asbd = cocoa::nonInterleavedFloatABSD( 2, 44100 );

		UInt32 busCount = mSources.size();
		OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &busCount, sizeof( busCount ) );

		float outputVolume = 1.0f;
		status = ::AudioUnitSetParameter( mAudioUnit, kMultiChannelMixerParam_Volume, kAudioUnitScope_Output, 0, outputVolume, 0 );
		CI_ASSERT( status == noErr );

		status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &asbd, sizeof( asbd ) );
		CI_ASSERT( status == noErr );

		for( UInt32 bus = 0; bus < busCount; bus++ ) {

			status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, bus, &asbd, sizeof( asbd ) );
			CI_ASSERT( status == noErr );

			float inputVolume = 1.0f;
			status = ::AudioUnitSetParameter( mAudioUnit, kMultiChannelMixerParam_Volume, kAudioUnitScope_Input, bus, inputVolume, 0 );
			CI_ASSERT( status == noErr );

//			status = ::AudioUnitSetParameter( mAudioUnit, kMultiChannelMixerParam_Enable, kAudioUnitScope_Input, bus, 1.0f, 0 );
//			CI_ASSERT( status == noErr );
		}

		status = ::AudioUnitInitialize( mAudioUnit );
		CI_ASSERT( status == noErr );

		LOG_V << "initialize complete. bus count: " << busCount << endl;
	}

// ----------------------------------------------------------------------------------------------------
// MARK: - GraphAudioUnit
// ----------------------------------------------------------------------------------------------------


	void GraphAudioUnit::initialize()
	{
		if( mInitialized )
			return;
		CI_ASSERT( mOutput );

		initializeNode( mOutput );

		size_t blockSize = mOutput->getBlockSize();
		mRenderContext.buffer.resize( mOutput->getFormat().getNumChannels() );
		for( auto& channel : mRenderContext.buffer )
			channel.resize( blockSize );
		mRenderContext.currentNode = mOutput.get();

		mInitialized = true;
		LOG_V << "graph initialize complete. output channels: " << mRenderContext.buffer.size() << ", blocksize: " << blockSize << endl;
	}

	void GraphAudioUnit::initializeNode( NodeRef node )
	{
		Node::Format& format = node->getFormat();

		for( NodeRef& sourceNode : node->getSources() )
			initializeNode( sourceNode );

		node->initialize();

		if( format.isNative() ) {
			::AudioUnit audioUnit = static_cast<::AudioUnit>( node->getNative() );
			CI_ASSERT( audioUnit );

			::AURenderCallbackStruct callbackStruct;
			callbackStruct.inputProc = GraphAudioUnit::renderCallback;
			callbackStruct.inputProcRefCon = &mRenderContext;

			for( UInt32 bus = 0; bus < node->getSources().size(); bus++ ) {
				OSStatus status = ::AudioUnitSetProperty( audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, bus, &callbackStruct, sizeof( callbackStruct ) );
				CI_ASSERT( status == noErr );
			}
		}
	}

	void GraphAudioUnit::uninitialize()
	{
		if( ! mInitialized )
			return;

		stop();
		if( mOutput )
			mOutput->uninitialize();
		mInitialized = false;
	}

	OSStatus GraphAudioUnit::renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList )
	{
		RenderContext *renderContext = static_cast<RenderContext *>( context );

		CI_ASSERT( bus < renderContext->currentNode->getSources().size() );
		CI_ASSERT( bufferList->mNumberBuffers == renderContext->buffer.size() );
		CI_ASSERT( numFrames == renderContext->buffer[0].size() ); // assumes non-interleaved

		NodeRef source = renderContext->currentNode->getSources()[bus];
		if( source->getFormat().isNative() ) {
			Node *thisNode = renderContext->currentNode;
			renderContext->currentNode = source.get();
			::AudioUnit audioUnit = static_cast<::AudioUnit>( source->getNative() );
			OSStatus status = ::AudioUnitRender( audioUnit, flags, timeStamp, bus, numFrames, bufferList );
			CI_ASSERT( status == noErr );

			renderContext->currentNode = thisNode; // reset context node for next iteration
		} else {
			source->render( &renderContext->buffer );

			// ???: how can I avoid this when generic nodes are chained together?
			// TODO: I can probably just memcpy all of mBuffer right over, but taking the safe route for now
			for( UInt32 i = 0; i < bufferList->mNumberBuffers; i++ ) {
				memcpy( bufferList->mBuffers[i].mData, renderContext->buffer[i].data(), bufferList->mBuffers[i].mDataByteSize );
			}
		}
		
		return noErr;
	}


} // namespace audio2