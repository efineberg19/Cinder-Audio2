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

		::AudioUnit outputAudioUnit = static_cast<::AudioUnit>( mDevice->getComponentInstance() );
		CI_ASSERT( outputAudioUnit ); // TODO: extract this first with a typecase to AudioUnit

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

		OSStatus status = ::AudioUnitSetProperty( outputAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, AudioUnitBus::Output, &mASBD, sizeof( mASBD ) );
		CI_ASSERT( status == noErr );

		mRenderContext = { this, &mBuffer };

		::AURenderCallbackStruct callbackStruct;
		callbackStruct.inputProc = OutputAudioUnit::renderCallback;
		callbackStruct.inputProcRefCon = &mRenderContext;

		status = ::AudioUnitSetProperty( outputAudioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, AudioUnitBus::Output, &callbackStruct, sizeof( callbackStruct ) );
		CI_ASSERT( status == noErr );

		mDevice->setOutputConnected();
		LOG_V << "OutputAudioUnit connected: input scope -> ouput bus" << endl;

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

	OSStatus OutputAudioUnit::renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList )
	{
		RenderContext *renderContext = static_cast<RenderContext *>( context );

//		OutputAudioUnit *self = static_cast<OutputAudioUnit *>( context );

		if( renderContext->node->getSources().empty() )
			return noErr;

		CI_ASSERT( bufferList->mNumberBuffers == renderContext->buffer->size() );
		CI_ASSERT( numFrames == renderContext->buffer->at( 0 ).size() ); // assumes non-interleaved

//		self->renderNode( self->mSources[0], &buffer, flags, timeStamp, busNumber, numFrames, bufferList );

		NodeRef source = renderContext->node->getSources().front();
		if( source->getFormat().isNative() ) {
			::AudioUnit audioUnit = static_cast<::AudioUnit>( source->getNative() );
			OSStatus status = ::AudioUnitRender( audioUnit, flags, timeStamp, busNumber, numFrames, bufferList );
			CI_ASSERT( status == noErr );
		} else {
			source->render( renderContext->buffer );

			// ???: how can I avoid this when generic nodes are chained together?
			// TODO: I can probably just memcpy all of mBuffer right over, but taking the safe route for now
			for( UInt32 i = 0; i < bufferList->mNumberBuffers; i++ ) {
				memcpy( bufferList->mBuffers[i].mData, renderContext->buffer->at( i ).data(), bufferList->mBuffers[i].mDataByteSize );
			}
		}

		return noErr;
	}

	// TODO: use kAudioUnitProperty_LastRenderError from outside renderloop, or if that doesn't work well use an atomic<OSStatus> that one can check from UI loop
	// FIXME: kAudioUnitErr_NoConnection for processor node - it appears the AudioUnitRender does the depth first travers itself, so this approach is doomed
	void OutputAudioUnit::renderNode( NodeRef node, BufferT *buffer, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *auBufferList )
	{
		if( ! node )
			return;

		// render children first
//		if( ! node->getSources().empty() )
//			renderNode( node->getSources().front(), buffer, flags, timeStamp, busNumber, numFrames, auBufferList );

//		if( node->getFormat().isNative() ) {
//			::AudioUnit audioUnit = static_cast<::AudioUnit>( node->getNative() );
//			OSStatus status = ::AudioUnitRender( audioUnit, flags, timeStamp, busNumber, numFrames, auBufferList );
//			CI_ASSERT( status == noErr );
//		} else {
//			node->render( buffer );
//
//			// ???: how can I avoid this when generic nodes are chained together?
//			// TODO: I can probably just memcpy all of mBuffer right over, but taking the safe route for now
//			for( UInt32 i = 0; i < auBufferList->mNumberBuffers; i++ ) {
//				memcpy( auBufferList->mBuffers[i].mData, buffer->at( i ).data(), auBufferList->mBuffers[i].mDataByteSize );
//			}
//		}
	}

// ----------------------------------------------------------------------------------------------------
// MARK: - ProcessorAudioUnit
// ----------------------------------------------------------------------------------------------------

	ProcessorAudioUnit::ProcessorAudioUnit(  UInt32 effectSubType )
	: mEffectSubType( effectSubType )
	{
		mTag = "ProcessorAudioUnit";
		mFormat.mIsNative = true;
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

		// TODO: some suggest AudioUnitGetProperty ( kAudioUnitProperty_StreamFormat ),  then set it's samplerate
//		mFormat.mNumChannels = source->getFormat().getNumChannels();
//		mFormat.mSampleRate = source->getFormat().getSampleRate();
		mFormat.mNumChannels = 2;
		mFormat.mSampleRate = 44100;
		::AudioStreamBasicDescription asbd = cocoa::nonInterleavedFloatABSD( mFormat.mNumChannels, mFormat.mSampleRate );

		OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, AudioUnitBus::Output, &asbd, sizeof( asbd ) );
		CI_ASSERT( status == noErr );

		// HACK to get internal buffer
		auto outputUnit = static_pointer_cast<OutputAudioUnit>( getParent() );
		CI_ASSERT( outputUnit );
		mRenderContext = { this, &outputUnit->getInternalBuffer() };

		::AURenderCallbackStruct callbackStruct;
		callbackStruct.inputProc = ProcessorAudioUnit::renderCallback;
		callbackStruct.inputProcRefCon = &mRenderContext;

		status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, AudioUnitBus::Output, &callbackStruct, sizeof( callbackStruct ) );
		CI_ASSERT( status == noErr );


		status = ::AudioUnitInitialize( mAudioUnit );
		CI_ASSERT( status == noErr );
	}

	void ProcessorAudioUnit::setParameter( ::AudioUnitParameterID param, float val )
	{
		OSStatus status = ::AudioUnitSetParameter( mAudioUnit, param, kAudioUnitScope_Global, 0, val, 0 );
		CI_ASSERT( status == noErr );
	}

	// TODO NEXT: test if this gets the effect working. For now if necessary, create a new BufferT and copy samples over
	OSStatus ProcessorAudioUnit::renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList )
	{
//		ProcessorAudioUnit *self = static_cast<ProcessorAudioUnit *>( context );
		RenderContext *renderContext = static_cast<RenderContext *>( context );

		if( renderContext->node->getSources().empty() )
			return noErr;

//		BufferT& buffer = self->mBuffer;
//		CI_ASSERT( bufferList->mNumberBuffers == buffer.size() );
//		CI_ASSERT( numFrames == buffer[0].size() ); // assumes non-interleaved
//
//		self->renderNode( self->mSources[0], &buffer, flags, timeStamp, busNumber, numFrames, bufferList );

		NodeRef source = renderContext->node->getSources().front();
		source->render( renderContext->buffer );

		// ???: how can I avoid this when generic nodes are chained together?
		// TODO: I can probably just memcpy all of mBuffer right over, but taking the safe route for now
		for( UInt32 i = 0; i < bufferList->mNumberBuffers; i++ ) {
			memcpy( bufferList->mBuffers[i].mData, renderContext->buffer->at( i ).data(), bufferList->mBuffers[i].mDataByteSize );
		}

		return noErr;
	}

} // namespace audio2