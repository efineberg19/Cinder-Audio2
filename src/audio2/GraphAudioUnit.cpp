#include "audio2/GraphAudioUnit.h"
#include "audio2/DeviceAudioUnit.h"
#include "audio2/audio.h"
#include "audio2/cocoa/Util.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

using namespace std;

namespace audio2 {

	template <typename ResultT>
	inline void audioUnitGetParam( ::AudioUnit audioUnit, ::AudioUnitParameterID property, ResultT &result, ::AudioUnitScope scope = kAudioUnitScope_Input, size_t bus = 0 )
	{
		::AudioUnitParameterValue param;
		::AudioUnitElement busElement = static_cast<::AudioUnitElement>( bus );
		OSStatus status = ::AudioUnitGetParameter( audioUnit, property, scope, busElement, &param );
		CI_ASSERT( status == noErr );
		result = static_cast<ResultT>( param );
	}

	template <typename ParamT>
	inline void audioUnitSetParam( ::AudioUnit audioUnit, ::AudioUnitParameterID property, ParamT param, ::AudioUnitScope scope = kAudioUnitScope_Input, size_t bus = 0 )
	{
		::AudioUnitParameterValue value = static_cast<::AudioUnitParameterValue>( param );
		::AudioUnitElement busElement = static_cast<::AudioUnitElement>( bus );
		OSStatus status = ::AudioUnitSetParameter( audioUnit, property, scope, busElement, value, 0 );
		CI_ASSERT( status == noErr );
	}

// ----------------------------------------------------------------------------------------------------
// MARK: - OutputAudioUnit
// ----------------------------------------------------------------------------------------------------

	OutputAudioUnit::OutputAudioUnit( DeviceRef device )
	: Output( device )
	{
		mTag = "OutputAudioUnit";
		mIsNative = true;
		mDevice = dynamic_pointer_cast<DeviceAudioUnit>( device );
		CI_ASSERT( mDevice );

		mFormat.setSampleRate( mDevice->getSampleRate() );
		mFormat.setNumChannels( 2 );
	}

	void OutputAudioUnit::initialize()
	{
		CI_ASSERT( ! mDevice->isOutputConnected() );

		::AudioUnit outputAudioUnit = static_cast<::AudioUnit>( mDevice->getComponentInstance() );
		CI_ASSERT( outputAudioUnit );

		::AudioStreamBasicDescription asbd = cocoa::nonInterleavedFloatABSD( mFormat.getNumChannels(), mFormat.getSampleRate() );

		OSStatus status = ::AudioUnitSetProperty( outputAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, AudioUnitBus::Output, &asbd, sizeof( asbd ) );
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
	: mEffectSubType( effectSubType ), mAudioUnit( nullptr )
	{
		mTag = "EffectAudioUnit";
		mIsNative = true;
	}

	EffectAudioUnit::~EffectAudioUnit()
	{
		if( mAudioUnit ) {
			OSStatus status = AudioComponentInstanceDispose( mAudioUnit );
			CI_ASSERT( status == noErr );
		}
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
		::AudioStreamBasicDescription asbd = cocoa::nonInterleavedFloatABSD( mFormat.getNumChannels(), mFormat.getSampleRate() );

		OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, AudioUnitBus::Output, &asbd, sizeof( asbd ) );
		CI_ASSERT( status == noErr );

		status = ::AudioUnitInitialize( mAudioUnit );
		CI_ASSERT( status == noErr );

		LOG_V << "initialize complete. " << endl;
	}

	void EffectAudioUnit::uninitialize()
	{
		OSStatus status = ::AudioUnitUninitialize( mAudioUnit );
		CI_ASSERT( status );
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
	: mAudioUnit( nullptr )
	{
		mTag = "MixerAudioUnit";
		mIsNative = true;
		mFormat.setWantsDefaultFormatFromParent();
	}

	MixerAudioUnit::~MixerAudioUnit()
	{
		if( mAudioUnit ) {
			OSStatus status = AudioComponentInstanceDispose( mAudioUnit );
			CI_ASSERT( status == noErr );
		}
	}

	void MixerAudioUnit::initialize()
	{
#if defined( CINDER_COCOA_TOUCH )
		if( mFormat.getNumChannels() > 2 )
			throw AudioParamExc( "iOS mult-channel mixer is limited to two output channels" );
#endif

		AudioComponentDescription comp{ 0 };
		comp.componentType = kAudioUnitType_Mixer;
		comp.componentSubType = kAudioUnitSubType_MultiChannelMixer;
		comp.componentManufacturer = kAudioUnitManufacturer_Apple;

		cocoa::findAndCreateAudioComponent( comp, &mAudioUnit );

		::AudioStreamBasicDescription asbd = cocoa::nonInterleavedFloatABSD( mFormat.getNumChannels(), mFormat.getSampleRate() );

		float outputVolume = 1.0f;
		OSStatus status = ::AudioUnitSetParameter( mAudioUnit, kMultiChannelMixerParam_Volume, kAudioUnitScope_Output, 0, outputVolume, 0 );
		CI_ASSERT( status == noErr );

		status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &asbd, sizeof( asbd ) );
		CI_ASSERT( status == noErr );

		CI_ASSERT( mSources.size() <= getNumBusses() );

		for( UInt32 bus = 0; bus < mSources.size(); bus++ ) {
			if( ! mSources[bus] )
				continue;

			Node::Format& sourceFormat = mSources[bus]->getFormat();
			::AudioStreamBasicDescription busAsbd = cocoa::nonInterleavedFloatABSD( sourceFormat.getNumChannels(), sourceFormat.getSampleRate() );

			status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, bus, &busAsbd, sizeof( busAsbd ) );
			CI_ASSERT( status == noErr );

			float inputVolume = 1.0f;
			status = ::AudioUnitSetParameter( mAudioUnit, kMultiChannelMixerParam_Volume, kAudioUnitScope_Input, bus, inputVolume, 0 );
			CI_ASSERT( status == noErr );
		}

		status = ::AudioUnitInitialize( mAudioUnit );
		CI_ASSERT( status == noErr );

		LOG_V << "initialize complete. " << endl;
	}

	void MixerAudioUnit::uninitialize()
	{
		OSStatus status = ::AudioUnitUninitialize( mAudioUnit );
		CI_ASSERT( status );
	}

	size_t MixerAudioUnit::getNumBusses()
	{
		UInt32 busCount;
		UInt32 busCountSize = sizeof( busCount );
		OSStatus status = ::AudioUnitGetProperty( mAudioUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &busCount, &busCountSize );
		CI_ASSERT( status == noErr );

		return static_cast<size_t>( busCount );
	}

	void MixerAudioUnit::setNumBusses( size_t count )
	{
		UInt32 busCount = static_cast<UInt32>( count );
		OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &busCount, sizeof( busCount ) );
		CI_ASSERT( status == noErr );
	}

	bool MixerAudioUnit::isBusEnabled( size_t bus )
	{
		checkBusIsValid( bus );

		::AudioUnitElement busElement = static_cast<::AudioUnitElement>( bus );
		::AudioUnitParameterValue enabledValue;
		OSStatus status = ::AudioUnitGetParameter( mAudioUnit, kMultiChannelMixerParam_Enable, kAudioUnitScope_Input, busElement, &enabledValue );
		CI_ASSERT( status == noErr );

		return static_cast<bool>( enabledValue );
	}

	void MixerAudioUnit::setBusEnabled( size_t bus, bool enabled )
	{
		checkBusIsValid( bus );

		::AudioUnitElement busElement = static_cast<::AudioUnitElement>( bus );
		::AudioUnitParameterValue enabledValue = static_cast<::AudioUnitParameterValue>( enabled );
		OSStatus status = ::AudioUnitSetParameter( mAudioUnit, kMultiChannelMixerParam_Enable, kAudioUnitScope_Input, busElement, enabledValue, 0 );
		CI_ASSERT( status == noErr );
	}

	void MixerAudioUnit::setBusVolume( size_t bus, float volume )
	{
		checkBusIsValid( bus );
		audioUnitSetParam( mAudioUnit, kMultiChannelMixerParam_Volume, volume, kAudioUnitScope_Input, bus );
	}

	float MixerAudioUnit::getBusVolume( size_t bus )
	{
		checkBusIsValid( bus );

		float volume;
		audioUnitGetParam( mAudioUnit, kMultiChannelMixerParam_Volume, volume, kAudioUnitScope_Input, bus );
		return volume;
	}

	void MixerAudioUnit::setBusPan( size_t bus, float pan )
	{
		checkBusIsValid( bus );
		audioUnitSetParam( mAudioUnit, kMultiChannelMixerParam_Pan, pan, kAudioUnitScope_Input, bus );
	}

	float MixerAudioUnit::getBusPan( size_t bus )
	{
		checkBusIsValid( bus );

		float pan;
		audioUnitGetParam( mAudioUnit, kMultiChannelMixerParam_Pan, pan, kAudioUnitScope_Input, bus );
		return pan;
	}

	void MixerAudioUnit::checkBusIsValid( size_t bus )
	{
		if( bus >= getNumBusses() )
			throw AudioParamExc( "Bus number out of range.");
	}

// ----------------------------------------------------------------------------------------------------
// MARK: - GraphAudioUnit
// ----------------------------------------------------------------------------------------------------

	GraphAudioUnit::~GraphAudioUnit()
	{
		
	}

	void GraphAudioUnit::initialize()
	{
		if( mInitialized )
			return;
		CI_ASSERT( mOutput );

		initNode( mOutput );

		size_t blockSize = mOutput->getBlockSize();
		mRenderContext.buffer.resize( mOutput->getFormat().getNumChannels() );
		for( auto& channel : mRenderContext.buffer )
			channel.resize( blockSize );
		mRenderContext.currentNode = mOutput.get();

		mInitialized = true;
		LOG_V << "graph initialize complete. output channels: " << mRenderContext.buffer.size() << ", blocksize: " << blockSize << endl;
	}

	void GraphAudioUnit::initNode( NodeRef node )
	{
		Node::Format& format = node->getFormat();

		if( ! format.isComplete() && format.wantsDefaultFormatFromParent() ) {
			NodeRef parent = node->getParent();
			while( parent ) {
				if( ! format.getSampleRate() )
					format.setSampleRate( parent->getFormat().getSampleRate() );
				if( ! format.getNumChannels() )
					format.setNumChannels( parent->getFormat().getNumChannels() );
				if( format.isComplete() )
					break;
				parent = parent->getParent();
			}
			CI_ASSERT( format.isComplete() );
		}

		for( NodeRef& sourceNode : node->getSources() )
			initNode( sourceNode );

		if( ! format.isComplete() && ! format.wantsDefaultFormatFromParent() ) {
			if( ! format.getSampleRate() )
				format.setSampleRate( node->getSourceFormat().getSampleRate() );
			if( ! format.getNumChannels() )
				format.setNumChannels( node->getSourceFormat().getNumChannels() );
		}
		CI_ASSERT( format.isComplete() );

		node->initialize();

		if( node->isNative() ) {
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
		uninitNode( mOutput );
		mInitialized = false;
	}

	void GraphAudioUnit::uninitNode( NodeRef node )
	{
		if( ! node )
			return;
		for( auto &source : node->getSources() )
			uninitNode( source );

		node->uninitialize();
	}

	OSStatus GraphAudioUnit::renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList )
	{
		RenderContext *renderContext = static_cast<RenderContext *>( context );

		CI_ASSERT( bus < renderContext->currentNode->getSources().size() );
		CI_ASSERT( bufferList->mNumberBuffers == renderContext->buffer.size() );
		CI_ASSERT( numFrames == renderContext->buffer[0].size() ); // assumes non-interleaved

		NodeRef source = renderContext->currentNode->getSources()[bus];
		if( source->isNative() ) {
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