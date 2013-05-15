#include "audio2/GraphAudioUnit.h"
#include "audio2/DeviceAudioUnit.h"
#include "audio2/audio.h"
#include "audio2/cocoa/Util.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

#include "cinder/Utilities.h"

using namespace std;

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - Audio Unit Helper Functions
// ----------------------------------------------------------------------------------------------------

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

inline ::AudioStreamBasicDescription getAudioUnitASBD( ::AudioUnit audioUnit, ::AudioUnitScope scope, ::AudioUnitElement bus = 0 ) {
	::AudioStreamBasicDescription result;
	UInt32 resultSize = sizeof( result );
	OSStatus status = AudioUnitGetProperty( audioUnit, kAudioUnitProperty_StreamFormat, scope, bus, &result,  &resultSize );
	CI_ASSERT( status == noErr );
	return result;
}

inline vector<::AUChannelInfo> getAudioUnitChannelInfo( ::AudioUnit audioUnit, ::AudioUnitElement bus = 0 ) {
	vector<::AUChannelInfo> result;
	UInt32 resultSize;
	OSStatus status = ::AudioUnitGetPropertyInfo( audioUnit, kAudioUnitProperty_SupportedNumChannels, kAudioUnitScope_Global, 0, &resultSize, NULL );
	if( status == kAudioUnitErr_InvalidProperty ) {
		// "if this property is NOT implemented an FX unit is expected to deal with same channel valance in and out" - CAPublicUtility / CAAudioUnit.cpp
		return result;
	} else
		CI_ASSERT( status == noErr );

	result.resize( resultSize / sizeof( ::AUChannelInfo ) );
	status = ::AudioUnitGetProperty( audioUnit, kAudioUnitProperty_SupportedNumChannels, kAudioUnitScope_Global, 0, result.data(), &resultSize );
	CI_ASSERT( status == noErr );

	return result;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - AudioUnitNode
// ----------------------------------------------------------------------------------------------------

AudioUnitNode::~AudioUnitNode()
{
	if( mAudioUnit ) {
		OSStatus status = AudioComponentInstanceDispose( mAudioUnit );
		CI_ASSERT( status == noErr );
	}
}

// ----------------------------------------------------------------------------------------------------
// MARK: - OutputAudioUnit
// ----------------------------------------------------------------------------------------------------

OutputAudioUnit::OutputAudioUnit( DeviceRef device )
: Output( device )
{
	mTag = "OutputAudioUnit";
	mDevice = dynamic_pointer_cast<DeviceAudioUnit>( device );
	CI_ASSERT( mDevice );

	mFormat.setSampleRate( mDevice->getSampleRate() );
	mFormat.setNumChannels( 2 );

	CI_ASSERT( ! mDevice->isOutputConnected() );
	mDevice->setOutputConnected();
}

void OutputAudioUnit::initialize()
{

	::AudioUnit audioUnit = getAudioUnit();
	CI_ASSERT( audioUnit );

	::AudioStreamBasicDescription asbd = cocoa::nonInterleavedFloatABSD( mFormat.getNumChannels(), mFormat.getSampleRate() );

	OSStatus status = ::AudioUnitSetProperty( audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, DeviceAudioUnit::Bus::Output, &asbd, sizeof( asbd ) );
	CI_ASSERT( status == noErr );

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

::AudioUnit OutputAudioUnit::getAudioUnit() const
{
	return mDevice->getComponentInstance();
}

size_t OutputAudioUnit::getBlockSize() const
{
	return mDevice->getBlockSize();
}

// ----------------------------------------------------------------------------------------------------
// MARK: - InputAudioUnit
// ----------------------------------------------------------------------------------------------------

// FIXME: in a multi-graph situation, Path A is only pliable if device is used for both I/O in _this_ graph
//	- this is only checking if I and O are both in use
//	- only way I can think of to solve this is to keep a weak reference to Graph in both I and O units,
//	  check graph->output->device in initialize to see if it's the same

InputAudioUnit::InputAudioUnit( DeviceRef device )
: Input( device )
{
	mTag = "InputAudioUnit";
	mRenderBus = DeviceAudioUnit::Bus::Input;

	mDevice = dynamic_pointer_cast<DeviceAudioUnit>( device );
	CI_ASSERT( mDevice );

	mFormat.setSampleRate( mDevice->getSampleRate() );
	mFormat.setNumChannels( 2 );

	CI_ASSERT( ! mDevice->isInputConnected() );
	mDevice->setInputConnected();
}

InputAudioUnit::~InputAudioUnit()
{
}

void InputAudioUnit::initialize()
{
	::AudioUnit audioUnit = getAudioUnit();
	CI_ASSERT( audioUnit );

	::AudioStreamBasicDescription asbd = cocoa::nonInterleavedFloatABSD( mFormat.getNumChannels(), mFormat.getSampleRate() );

	OSStatus status = ::AudioUnitSetProperty( audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, DeviceAudioUnit::Bus::Input, &asbd, sizeof( asbd ) );
	CI_ASSERT( status == noErr );

	if( mDevice->isOutputConnected() ) {
		LOG_V << "Path A. The High Road." << endl;
		// output node is expected to initialize device, since it is pulling all the way to here.
	}
	else {
		LOG_V << "Path B. initiate ringbuffer" << endl;
		mShouldUseGraphRenderCallback = false;

		mRingBuffer = unique_ptr<RingBuffer>( new RingBuffer( mDevice->getBlockSize() * mFormat.getNumChannels() ) );
		mBufferList = cocoa::createNonInterleavedBufferList( mFormat.getNumChannels(), mDevice->getBlockSize() * sizeof( float ) );

		::AURenderCallbackStruct callbackStruct;
		callbackStruct.inputProc = InputAudioUnit::inputCallback;
		callbackStruct.inputProcRefCon = this;
		status = ::AudioUnitSetProperty( audioUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, DeviceAudioUnit::Bus::Input, &callbackStruct, sizeof( callbackStruct ) );
		CI_ASSERT( status == noErr );

		mDevice->initialize();
	}

	LOG_V << "initialize complete." << endl;
}

void InputAudioUnit::uninitialize()
{
	mDevice->uninitialize();
}

void InputAudioUnit::start()
{

	if( ! mDevice->isOutputConnected() ) {
		mDevice->start();
		LOG_V << "started: " << mDevice->getName() << endl;
	}
}

void InputAudioUnit::stop()
{
	if( ! mDevice->isOutputConnected() ) {
		mDevice->stop();
		LOG_V << "stopped: " << mDevice->getName() << endl;
	}
}

DeviceRef InputAudioUnit::getDevice()
{
	return std::static_pointer_cast<Device>( mDevice );
}

::AudioUnit InputAudioUnit::getAudioUnit() const
{
	return mDevice->getComponentInstance();
}

void InputAudioUnit::render( BufferT *buffer )
{
	CI_ASSERT( mRingBuffer );

	size_t numFrames = buffer->at( 0 ).size();
	for( size_t c = 0; c < buffer->size(); c++ ) {
		size_t count = mRingBuffer->read( &(*buffer)[c] );
		if( count != numFrames )
			LOG_V << " Warning, unexpected read count: " << count << ", expected: " << numFrames << " (c = " << c << ")" << endl;
	}
}

OSStatus InputAudioUnit::inputCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList )
{
	InputAudioUnit *inputNode = static_cast<InputAudioUnit *>( context );
	CI_ASSERT( inputNode->mRingBuffer );
	
	::AudioBufferList *nodeBufferList = inputNode->mBufferList.get();
	OSStatus status = ::AudioUnitRender( inputNode->getAudioUnit(), flags, timeStamp, DeviceAudioUnit::Bus::Input, numFrames, nodeBufferList );
	CI_ASSERT( status == noErr );

	for( size_t c = 0; c < nodeBufferList->mNumberBuffers; c++ ) {
		float *channel = static_cast<float *>( nodeBufferList->mBuffers[c].mData );
		inputNode->mRingBuffer->write( channel, numFrames );
	}
	return status;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - EffectAudioUnit
// ----------------------------------------------------------------------------------------------------

EffectAudioUnit::EffectAudioUnit(  UInt32 effectSubType )
: mEffectSubType( effectSubType )
{
	mTag = "EffectAudioUnit";
}

EffectAudioUnit::~EffectAudioUnit()
{
}

void EffectAudioUnit::initialize()
{
	::AudioComponentDescription comp{ 0 };
	comp.componentType = kAudioUnitType_Effect;
	comp.componentSubType = mEffectSubType;
	comp.componentManufacturer = kAudioUnitManufacturer_Apple;
	cocoa::findAndCreateAudioComponent( comp, &mAudioUnit );

	auto source = mSources.front();
	CI_ASSERT( source );

	::AudioStreamBasicDescription asbd = cocoa::nonInterleavedFloatABSD( mFormat.getNumChannels(), mFormat.getSampleRate() );
	OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &asbd, sizeof( asbd ) );
	CI_ASSERT( status == noErr );
	status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &asbd, sizeof( asbd ) );
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
{
	mTag = "MixerAudioUnit";
	mFormat.setWantsDefaultFormatFromParent();
}

MixerAudioUnit::~MixerAudioUnit()
{		
}

void MixerAudioUnit::initialize()
{
#if defined( CINDER_COCOA_TOUCH )
	if( mFormat.getNumChannels() > 2 )
		throw AudioParamExc( "iOS mult-channel mixer is limited to two output channels" );
#endif

	::AudioComponentDescription comp{ 0 };
	comp.componentType = kAudioUnitType_Mixer;
	comp.componentSubType = kAudioUnitSubType_MultiChannelMixer;
	comp.componentManufacturer = kAudioUnitManufacturer_Apple;

	cocoa::findAndCreateAudioComponent( comp, &mAudioUnit );

	::AudioStreamBasicDescription asbd = cocoa::nonInterleavedFloatABSD( mFormat.getNumChannels(), mFormat.getSampleRate() );
	OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &asbd, sizeof( asbd ) );
	CI_ASSERT( status == noErr );

	float outputVolume = 1.0f;
	status = ::AudioUnitSetParameter( mAudioUnit, kMultiChannelMixerParam_Volume, kAudioUnitScope_Output, 0, outputVolume, 0 );
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
// MARK: - ConverterAudioUnit
// ----------------------------------------------------------------------------------------------------

ConverterAudioUnit::ConverterAudioUnit( NodeRef source, NodeRef dest, size_t outputBlockSize )
{
	mTag = "ConverterAudioUnit";
	mFormat = dest->getFormat();
	mSourceFormat = source->getFormat();
	mSources.resize( 1 );

	mRenderContext.currentNode = this;
	mRenderContext.buffer.resize( mSourceFormat.getNumChannels() );
	for( auto& channel : mRenderContext.buffer )
		channel.resize( outputBlockSize );
}

ConverterAudioUnit::~ConverterAudioUnit()
{
}

void ConverterAudioUnit::initialize()
{
	AudioComponentDescription comp{ 0 };
	comp.componentType = kAudioUnitType_FormatConverter;
	comp.componentSubType = kAudioUnitSubType_AUConverter;
	comp.componentManufacturer = kAudioUnitManufacturer_Apple;

	cocoa::findAndCreateAudioComponent( comp, &mAudioUnit );

	::AudioStreamBasicDescription inputAsbd = cocoa::nonInterleavedFloatABSD( mSourceFormat.getNumChannels(), mSourceFormat.getSampleRate() );
	::AudioStreamBasicDescription outputAsbd = cocoa::nonInterleavedFloatABSD( mFormat.getNumChannels(), mFormat.getSampleRate() );

	LOG_V << "input ASBD:" << endl;
	cocoa::printASBD( inputAsbd );
	LOG_V << "output ASBD:" << endl;
	cocoa::printASBD( outputAsbd );


	OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &outputAsbd, sizeof( outputAsbd ) );
	CI_ASSERT( status == noErr );

	status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &inputAsbd, sizeof( inputAsbd ) );
	CI_ASSERT( status == noErr );

	if( mSourceFormat.getNumChannels() == 1 && mFormat.getNumChannels() == 2 ) {
		// map mono source to stereo out
		UInt32 channelMap[2] = { 0, 0 };
		status = ::AudioUnitSetProperty( mAudioUnit, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Output, 0, &channelMap, sizeof( channelMap ) );
		CI_ASSERT( status == noErr );
	}

	status = ::AudioUnitInitialize( mAudioUnit );
	CI_ASSERT( status == noErr );

	LOG_V << "initialize complete. " << endl;
}

void ConverterAudioUnit::uninitialize()
{
	OSStatus status = ::AudioUnitUninitialize( mAudioUnit );
	CI_ASSERT( status );
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

	// set default params from parent if requested
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

	// recurse through sources
	for( NodeRef& sourceNode : node->getSources() )
		initNode( sourceNode );

	// set default params from source
	if( ! format.isComplete() && ! format.wantsDefaultFormatFromParent() ) {
		if( ! format.getSampleRate() )
			format.setSampleRate( node->getSourceFormat().getSampleRate() );
		if( ! format.getNumChannels() )
			format.setNumChannels( node->getSourceFormat().getNumChannels() );
	}

	CI_ASSERT( format.isComplete() );

	for( size_t bus = 0; bus < node->getSources().size(); bus++ ) {
		NodeRef& sourceNode = node->getSources()[bus];
		bool needsConverter = false;
		if( format.getSampleRate() != sourceNode->getFormat().getSampleRate() )
#if 0
			needsConverter = true;
#else
			throw AudioFormatExc( "non-matching samplerates not supported" );
#endif
		if( format.getNumChannels() != sourceNode->getFormat().getNumChannels() ) {
			LOG_V << "CHANNEL MISMATCH: " << sourceNode->getFormat().getNumChannels() << " -> " << format.getNumChannels() << endl;
			// TODO: if node is an OutputAudioUnit, or Mixer, they can do the channel mapping and avoid the converter
			needsConverter = true;
		}
		if( needsConverter ) {
			auto converter = make_shared<ConverterAudioUnit>( sourceNode, node, mOutput->getBlockSize() );
			converter->getSources()[0] = sourceNode;
			node->getSources()[bus] = converter;
			converter->setParent( node->getSources()[bus] );
			converter->initialize();
			connectRenderCallback( converter, &converter->mRenderContext, true ); // TODO: make sure this doesn't blow away other converters
		}
	}

	node->initialize();

	connectRenderCallback( node );
}

// TODO: if both node and source are native, consider directly connecting instead of using render callback - diffuculty here is knowing when to use the generic render()
void GraphAudioUnit::connectRenderCallback( NodeRef node, RenderContext *context, bool recursive )
{
	AudioUnitNode *nodeAU = dynamic_cast<AudioUnitNode *>( node.get() );
	if( ! nodeAU || ! nodeAU->shouldUseGraphRenderCallback() )
		return;

	::AudioUnit audioUnit = nodeAU->getAudioUnit();
	CI_ASSERT( audioUnit );

	// DEBUG: print channel info, although it has always been empty for me so far
	vector<::AUChannelInfo> channelInfo = getAudioUnitChannelInfo( audioUnit );
	if( ! channelInfo.empty() ) {
		LOG_V << "AUChannelInfo: " << endl;
		for( auto& ch : channelInfo )
			ci::app::console() << "\t ins: " << ch.inChannels << ", outs: " << ch.outChannels << endl;
	}

	::AURenderCallbackStruct callbackStruct;
	callbackStruct.inputProc = GraphAudioUnit::renderCallback;
	callbackStruct.inputProcRefCon = ( context ? context : &mRenderContext );

	for( UInt32 bus = 0; bus < node->getSources().size(); bus++ ) {
		OSStatus status = ::AudioUnitSetProperty( audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, bus, &callbackStruct, sizeof( callbackStruct ) );
		LOG_V << "connected render callback to: " << node->getSources()[bus]->getTag() << endl;
		CI_ASSERT( status == noErr );

		if( recursive )
			connectRenderCallback( node->getSources()[bus], context, true );
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

// TODO: try to avoid multiple copies when generic nodes are chained together
OSStatus GraphAudioUnit::renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList )
{
	RenderContext *renderContext = static_cast<RenderContext *>( context );

	CI_ASSERT( bus < renderContext->currentNode->getSources().size() );
	CI_ASSERT( bufferList->mNumberBuffers == renderContext->buffer.size() );

	// note: if samplerate conversion is allowed, this size may need to vary
	CI_ASSERT( numFrames <= renderContext->buffer[0].size() ); // assumes non-interleaved

	NodeRef source = renderContext->currentNode->getSources()[bus];

	AudioUnitNode *sourceAU = dynamic_cast<AudioUnitNode *>( source.get() );

	// check if this needs native rendering
	if( sourceAU && sourceAU->shouldUseGraphRenderCallback() ) {
		Node *thisNode = renderContext->currentNode;
		renderContext->currentNode = source.get();
		::AudioUnit audioUnit = sourceAU->getAudioUnit();
		::AudioUnitScope renderBus = sourceAU->getRenderBus();
		OSStatus status = ::AudioUnitRender( audioUnit, flags, timeStamp, renderBus, numFrames, bufferList );
		CI_ASSERT( status == noErr );

		renderContext->currentNode = thisNode;
	}
	else {
		// render all children through this callback, since there is a possiblity they can fall into the native category
		bool didRenderChildren = false;
		for( size_t i = 0; i < source->getSources().size(); i++ ) {
			didRenderChildren = true;
			Node *thisNode = renderContext->currentNode;
			renderContext->currentNode = source.get();

			renderCallback( renderContext, flags, timeStamp, 0, numFrames, bufferList );

			renderContext->currentNode = thisNode;
		}

		if( didRenderChildren ) {
			// copy samples from AudioBufferList to the generic buffer before generic render
			for( UInt32 i = 0; i < bufferList->mNumberBuffers; i++ ) {
				memcpy( renderContext->buffer[i].data(), bufferList->mBuffers[i].mData, bufferList->mBuffers[i].mDataByteSize );
			}
		}
		
		source->render( &renderContext->buffer );

		// now copy samples back to the output buffer
		for( UInt32 i = 0; i < bufferList->mNumberBuffers; i++ ) {
			memcpy( bufferList->mBuffers[i].mData, renderContext->buffer[i].data(), bufferList->mBuffers[i].mDataByteSize );
		}
	}
	
	return noErr;
}

} // namespace audio2