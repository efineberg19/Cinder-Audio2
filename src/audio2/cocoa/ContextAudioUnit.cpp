#include "audio2/cocoa/ContextAudioUnit.h"
#include "audio2/cocoa/DeviceAudioUnit.h"
#include "audio2/cocoa/CinderCoreAudio.h"
#include "audio2/audio.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

#include "cinder/Utilities.h"

using namespace std;

namespace audio2 { namespace cocoa {

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
	OSStatus status = ::AudioUnitGetProperty( audioUnit, kAudioUnitProperty_StreamFormat, scope, bus, &result,  &resultSize );
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
		OSStatus status = ::AudioComponentInstanceDispose( mAudioUnit );
		CI_ASSERT( status == noErr );
	}
}

// ----------------------------------------------------------------------------------------------------
// MARK: - OutputAudioUnit
// ----------------------------------------------------------------------------------------------------

OutputAudioUnit::OutputAudioUnit( DeviceRef device, const Format &format )
: OutputNode( device, format )
{
	mTag = "OutputAudioUnit";
	mDevice = dynamic_pointer_cast<DeviceAudioUnit>( device );
	CI_ASSERT( mDevice );

	if( mNumChannelsUnspecified )
		setNumChannels( 2 );

	CI_ASSERT( ! mDevice->isOutputConnected() );
	mDevice->setOutputConnected();
}

void OutputAudioUnit::initialize()
{

	::AudioUnit audioUnit = getAudioUnit();
	CI_ASSERT( audioUnit );

	::AudioStreamBasicDescription asbd = cocoa::createFloatAsbd( getNumChannels(), getContext()->getSampleRate() );

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

// ----------------------------------------------------------------------------------------------------
// MARK: - InputAudioUnit
// ----------------------------------------------------------------------------------------------------

// FIXME: in a multi-graph situation, Path A is only pliable if device is used for both I/O in _this_ graph
//	- this is only checking if I and O are both in use
//	- only way I can think of to solve this is to keep a weak reference to Graph in both I and O units,
//	  check graph->output->device in initialize to see if it's the same

InputAudioUnit::InputAudioUnit( DeviceRef device, const Format &format )
: InputNode( device, format )
{
	mTag = "InputAudioUnit";
	mRenderBus = DeviceAudioUnit::Bus::Input;

	mDevice = dynamic_pointer_cast<DeviceAudioUnit>( device );
	CI_ASSERT( mDevice );

	if( mNumChannelsUnspecified ) {
		setNumChannels( 2 ); // TODO: should default input channels be 1?
	}

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

	::AudioStreamBasicDescription asbd = cocoa::createFloatAsbd( getNumChannels(), getContext()->getSampleRate() );

	OSStatus status = ::AudioUnitSetProperty( audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, DeviceAudioUnit::Bus::Input, &asbd, sizeof( asbd ) );
	CI_ASSERT( status == noErr );

	if( mDevice->isOutputConnected() ) {
		LOG_V << "Path A. The High Road." << endl;
		// output node is expected to initialize device, since it is pulling all the way to here.
	}
	else {
		LOG_V << "Path B. initiate ringbuffer" << endl;
		mShouldUseGraphRenderCallback = false;

		mRingBuffer = unique_ptr<RingBuffer>( new RingBuffer( mDevice->getNumFramesPerBlock() * getNumChannels() ) );
		mBufferList = cocoa::createNonInterleavedBufferList( getNumChannels(), mDevice->getNumFramesPerBlock() * sizeof( float ) );

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
		mEnabled = true;
		mDevice->start();
		LOG_V << "started: " << mDevice->getName() << endl;
	}
}

void InputAudioUnit::stop()
{
	if( ! mDevice->isOutputConnected() ) {
		mEnabled = false;
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

void InputAudioUnit::process( Buffer *buffer )
{
	CI_ASSERT( mRingBuffer );

	size_t numFrames = buffer->getNumFrames();
	for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ ) {
		size_t count = mRingBuffer->read( buffer->getChannel( ch ), numFrames );
		if( count != numFrames )
			LOG_V << " Warning, unexpected read count: " << count << ", expected: " << numFrames << " (ch = " << ch << ")" << endl;
	}
}

OSStatus InputAudioUnit::inputCallback( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList )
{
	InputAudioUnit *inputNode = static_cast<InputAudioUnit *>( data );
	CI_ASSERT( inputNode->mRingBuffer );
	
	::AudioBufferList *nodeBufferList = inputNode->mBufferList.get();
	OSStatus status = ::AudioUnitRender( inputNode->getAudioUnit(), flags, timeStamp, DeviceAudioUnit::Bus::Input, numFrames, nodeBufferList );
	CI_ASSERT( status == noErr );

	for( size_t ch = 0; ch < nodeBufferList->mNumberBuffers; ch++ ) {
		float *channel = static_cast<float *>( nodeBufferList->mBuffers[ch].mData );
		inputNode->mRingBuffer->write( channel, numFrames );
	}
	return status;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - EffectAudioUnit
// ----------------------------------------------------------------------------------------------------

EffectAudioUnit::EffectAudioUnit(  UInt32 effectSubType, const Format &format )
: EffectNode( format ), mEffectSubType( effectSubType )
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

	::AudioStreamBasicDescription asbd = cocoa::createFloatAsbd( getNumChannels(), getContext()->getSampleRate() );
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
	CI_ASSERT( status == noErr );
}

void EffectAudioUnit::setParameter( ::AudioUnitParameterID param, float val )
{
	OSStatus status = ::AudioUnitSetParameter( mAudioUnit, param, kAudioUnitScope_Global, 0, val, 0 );
	CI_ASSERT( status == noErr );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - MixerAudioUnit
// ----------------------------------------------------------------------------------------------------

MixerAudioUnit::MixerAudioUnit( const Format &format )
: MixerNode( format )
{
	mTag = "MixerAudioUnit";
	mWantsDefaultFormatFromParent = true;
}

MixerAudioUnit::~MixerAudioUnit()
{		
}

void MixerAudioUnit::initialize()
{
#if defined( CINDER_COCOA_TOUCH )
	if( getNumChannels() > 2 )
		throw AudioParamExc( "iOS mult-channel mixer is limited to two output channels" );
#endif

	::AudioComponentDescription comp{ 0 };
	comp.componentType = kAudioUnitType_Mixer;
	comp.componentSubType = kAudioUnitSubType_MultiChannelMixer;
	comp.componentManufacturer = kAudioUnitManufacturer_Apple;

	cocoa::findAndCreateAudioComponent( comp, &mAudioUnit );

	size_t sampleRate = getContext()->getSampleRate();
	::AudioStreamBasicDescription asbd = cocoa::createFloatAsbd( getNumChannels(), sampleRate );
	OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &asbd, sizeof( asbd ) );
	CI_ASSERT( status == noErr );

	float outputVolume = 1.0f;
	status = ::AudioUnitSetParameter( mAudioUnit, kMultiChannelMixerParam_Volume, kAudioUnitScope_Output, 0, outputVolume, 0 );
	CI_ASSERT( status == noErr );

	if( mSources.size() > getNumBusses() ) {
		setMaxNumBusses( mSources.size() );
	}

	for( UInt32 bus = 0; bus < mSources.size(); bus++ ) {
		if( ! mSources[bus] )
			continue;

		::AudioStreamBasicDescription busAsbd = cocoa::createFloatAsbd( mSources[bus]->getNumChannels(), sampleRate );

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
	CI_ASSERT( status == noErr );
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
	// TODO: this method should probably be removed
}

void MixerAudioUnit::setMaxNumBusses( size_t count )
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

// TODO: remove these params - they are deduced once connected
ConverterAudioUnit::ConverterAudioUnit( NodeRef source, NodeRef dest )
: Node( Format() )
{
	mTag = "ConverterAudioUnit";

	mNumChannels = dest->getNumChannels();
	mSourceNumChannels = source->getNumChannels();
}

ConverterAudioUnit::~ConverterAudioUnit()
{
}

// TODO NEXT: see if this guy can handle non-interleaved -> interleaved and back
// if so, can install ConverterAudio unit's at either side to deliver proper buffer layout
// - but what about the end Buffer - how to provide one with Layout::Interleaved?
void ConverterAudioUnit::initialize()
{
	::AudioComponentDescription comp{ 0 };
	comp.componentType = kAudioUnitType_FormatConverter;
	comp.componentSubType = kAudioUnitSubType_AUConverter;
	comp.componentManufacturer = kAudioUnitManufacturer_Apple;

	NodeRef sourceNode = mSources[0];
	NodeRef destNode = getParent();

	mBufferLayout = destNode->getBufferLayout();

	mRenderContext.currentNode = this;
	mRenderContext.buffer = Buffer( sourceNode->getNumChannels(), getContext()->getNumFramesPerBlock(), sourceNode->getBufferLayout() );


	cocoa::findAndCreateAudioComponent( comp, &mAudioUnit );

	size_t sampleRate = getContext()->getSampleRate();
	::AudioStreamBasicDescription inputAsbd = cocoa::createFloatAsbd( sourceNode->getNumChannels(), sampleRate, ( sourceNode->getBufferLayout() == Buffer::Layout::Interleaved ) );
	::AudioStreamBasicDescription outputAsbd = cocoa::createFloatAsbd( destNode->getNumChannels(), sampleRate, ( destNode->getBufferLayout() == Buffer::Layout::Interleaved ) );

	LOG_V << "input ASBD:" << endl;
	cocoa::printASBD( inputAsbd );
	LOG_V << "output ASBD:" << endl;
	cocoa::printASBD( outputAsbd );


	OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &outputAsbd, sizeof( outputAsbd ) );
	CI_ASSERT( status == noErr );

	status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &inputAsbd, sizeof( inputAsbd ) );
	CI_ASSERT( status == noErr );

	if( mSourceNumChannels == 1 && getNumChannels() == 2 ) {
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
	CI_ASSERT( status == noErr );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - ContextAudioUnit
// ----------------------------------------------------------------------------------------------------

ContextAudioUnit::~ContextAudioUnit()
{
	if( mInitialized )
		uninitialize();
}

void ContextAudioUnit::initialize()
{
	if( mInitialized )
		return;
	CI_ASSERT( mRoot );

	mSampleRate = mRoot->getSampleRate();
	mNumFramesPerBlock = mRoot->getNumFramesPerBlock();

	initNode( mRoot );

	mRenderContext.buffer = Buffer( mRoot->getNumChannels(), mNumFramesPerBlock );
	mRenderContext.currentNode = mRoot.get();

	connectRenderCallback( mRoot, &mRenderContext, &ContextAudioUnit::renderCallbackRoot, false );

	mInitialized = true;
	LOG_V << "graph initialize complete. output channels: " << mRenderContext.buffer.getNumChannels() << ", frames per block: " << mRenderContext.buffer.getNumFrames() << endl;
}

void ContextAudioUnit::initNode( NodeRef node )
{
	if( ! node )
		return;

	node->setContext( shared_from_this() );

	if( node->getWantsDefaultFormatFromParent() && node->isNumChannelsUnspecified() )
		node->fillFormatParamsFromParent();

	// recurse through sources
	for( NodeRef& sourceNode : node->getSources() )
		initNode( sourceNode );

	// set default params from source
	if(  ! node->getWantsDefaultFormatFromParent() && node->isNumChannelsUnspecified() )
		node->fillFormatParamsFromSource();

	for( size_t bus = 0; bus < node->getSources().size(); bus++ ) {
		NodeRef sourceNode = node->getSources()[bus];
		if( ! sourceNode )
			continue;

		// TODO: if node is an OutputAudioUnit, or Mixer, they can do the channel mapping and avoid the converter
		bool needsConverter = false;
		if( node->getNumChannels() != sourceNode->getNumChannels() )
			needsConverter = true;
		else if( node->getBufferLayout() != sourceNode->getBufferLayout() )
			needsConverter = true;

		if( needsConverter ) {
			auto converter = make_shared<ConverterAudioUnit>( sourceNode, node );
			converter->setContext( shared_from_this() );
			node->setSource( converter, bus );
			converter->setSource( sourceNode );
			converter->initialize();
			connectRenderCallback( converter, &converter->mRenderContext, &ContextAudioUnit::renderCallbackConverter, true );
		}
	}

	node->initialize();

	connectRenderCallback( node, &mRenderContext, &ContextAudioUnit::renderCallback, false );
}

// TODO: if both node and source are native, consider directly connecting instead of using render callback - diffuculty here is knowing when to use the generic process()
void ContextAudioUnit::connectRenderCallback( NodeRef node, RenderContext *context, ::AURenderCallback callback, bool recursive )
{
	CI_ASSERT( context );

	AudioUnitNode *nodeAU = dynamic_cast<AudioUnitNode *>( node.get() );
	if( ! nodeAU || ! nodeAU->shouldUseGraphRenderCallback() )
		return;

	::AudioUnit audioUnit = nodeAU->getAudioUnit();
	CI_ASSERT( audioUnit );

	::AURenderCallbackStruct callbackStruct = { callback, context };

	for( UInt32 bus = 0; bus < node->getSources().size(); bus++ ) {
		NodeRef source = node->getSources()[bus];
		if( ! source )
			continue;
		OSStatus status = ::AudioUnitSetProperty( audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, bus, &callbackStruct, sizeof( callbackStruct ) );
		CI_ASSERT( status == noErr );
		LOG_V << "connected render callback to: " << source->getTag() << endl;

		if( recursive )
			connectRenderCallback( source, context, callback, recursive );
	}
}

void ContextAudioUnit::uninitialize()
{
	if( ! mInitialized )
		return;

	LOG_V << "uninitializing..." << endl;

	stop();
	uninitNode( mRoot );
	mInitialized = false;

	LOG_V << "done." << endl;
}

void ContextAudioUnit::uninitNode( NodeRef node )
{
	if( ! node )
		return;
	for( auto &source : node->getSources() )
		uninitNode( source );

	node->uninitialize();

	// throw away any ConverterNodes
	ConverterAudioUnit *converter = dynamic_cast<ConverterAudioUnit *>( node.get() );
	if( converter )
		converter->getParent()->setSource( converter->getSources()[0] );
}

// TODO: consider adding a volume param here
// - OS X output unit has kHALOutputParam_Volume, but need to check if this works on iOS
// - this is also made difficult because I'm currently connecting the ConverterNode's callback to this
OSStatus ContextAudioUnit::renderCallbackRoot( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList )
{
	static_cast<RenderContext *>( data )->buffer.zero();

	// TODO: zero out bufferList
	
	return renderCallback( data, flags, timeStamp, busNumber, numFrames, bufferList );
}

OSStatus ContextAudioUnit::renderCallbackConverter( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList )
{
	static_cast<RenderContext *>( data )->buffer.zero();
	return renderCallback( data, flags, timeStamp, busNumber, numFrames, bufferList );
}

// FIXME: in rare cases when all nodes are disabled, bufferList can contain crap  and needs to be zerod out

// TODO: avoid multiple copies when generic nodes are chained together
OSStatus ContextAudioUnit::renderCallback( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList )
{
	RenderContext *renderContext = static_cast<RenderContext *>( data );

	CI_ASSERT( renderContext->currentNode );
	CI_ASSERT( bus < renderContext->currentNode->getSources().size() );
	CI_ASSERT( numFrames == renderContext->buffer.getNumFrames() );

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
			if( ! source->getSources()[i] )
				continue;

			didRenderChildren = true;
			Node *thisNode = renderContext->currentNode;
			renderContext->currentNode = source.get();

			renderCallback( renderContext, flags, timeStamp, 0, numFrames, bufferList );

			renderContext->currentNode = thisNode;
		}

		if( didRenderChildren ) {
			// copy samples from AudioBufferList to the generic buffer before generic render
			// TODO: consider adding methods for buffer copying to CinderCoreAudio
			if( renderContext->buffer.getLayout() == Buffer::Layout::Interleaved ) {
				CI_ASSERT( bufferList->mNumberBuffers == 1 );
				memcpy( renderContext->buffer.getData(), bufferList->mBuffers[0].mData, bufferList->mBuffers[0].mDataByteSize );
			}
			else {
				for( UInt32 i = 0; i < bufferList->mNumberBuffers; i++ )
					memcpy( renderContext->buffer.getChannel( i ), bufferList->mBuffers[i].mData, bufferList->mBuffers[i].mDataByteSize );
			}
		}

		if( source->isEnabled() ) {
			source->process( &renderContext->buffer );

			// copy samples back to the output buffer
			if( renderContext->buffer.getLayout() == Buffer::Layout::Interleaved ) {
				CI_ASSERT( bufferList->mNumberBuffers == 1 );
				memcpy( bufferList->mBuffers[0].mData, renderContext->buffer.getData(), bufferList->mBuffers[0].mDataByteSize );
			}
			else {
				for( UInt32 i = 0; i < bufferList->mNumberBuffers; i++ )
					memcpy( bufferList->mBuffers[i].mData, renderContext->buffer.getChannel( i ), bufferList->mBuffers[i].mDataByteSize );
			}
		}
	}
	
	return noErr;
}

} } // namespace audio2::cocoa