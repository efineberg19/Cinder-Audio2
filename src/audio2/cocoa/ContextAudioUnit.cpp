/*
 Copyright (c) 2013, The Cinder Project

 This code is intended to be used with the Cinder C++ library, http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#include "audio2/cocoa/ContextAudioUnit.h"
#include "audio2/cocoa/DeviceAudioUnit.h"
#include "audio2/cocoa/CinderCoreAudio.h"
#include "audio2/audio.h"
#include "audio2/CinderAssert.h"
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

void checkBufferListNotClipping( const AudioBufferList *bufferList, UInt32 numFrames )
{
	const float kClipLimit = 2.0f;
	for( UInt32 c = 0; c < bufferList->mNumberBuffers; c++ ) {
		float *buf = (float *)bufferList->mBuffers[c].mData;
		for( UInt32 i = 0; i < numFrames; i++ )
			CI_ASSERT_MSG( buf[i] < kClipLimit, "Audio Clipped" );
	}
}

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeAudioUnit
// ----------------------------------------------------------------------------------------------------

NodeAudioUnit::~NodeAudioUnit()
{
	if( mAudioUnit ) {
		OSStatus status = ::AudioComponentInstanceDispose( mAudioUnit );
		CI_ASSERT( status == noErr );
	}
}

// ----------------------------------------------------------------------------------------------------
// MARK: - LineOutAudioUnit
// ----------------------------------------------------------------------------------------------------

LineOutAudioUnit::LineOutAudioUnit( DeviceRef device, const Format &format )
: LineOutNode( device, format )
{
	mDevice = dynamic_pointer_cast<DeviceAudioUnit>( device );
	CI_ASSERT( mDevice );

	if( mChannelMode != ChannelMode::SPECIFIED ) {
		mChannelMode = ChannelMode::SPECIFIED;
		setNumChannels( 2 );
	}
}

void LineOutAudioUnit::initialize()
{
	// LineOut always needs an internal buffer to deliver to the ouput AU, so force one to be made.
	setProcessWithSumming();

	mDevice->setOutputConnected();

	mRenderContext.node = this;
	mRenderContext.context = dynamic_cast<ContextAudioUnit *>( getContext().get() );

	::AudioUnit audioUnit = getAudioUnit();
	CI_ASSERT( audioUnit );

	::AudioStreamBasicDescription asbd = cocoa::createFloatAsbd( getNumChannels(), getContext()->getSampleRate() );

	OSStatus status = ::AudioUnitSetProperty( getAudioUnit(), kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, DeviceAudioUnit::Bus::Output, &asbd, sizeof( asbd ) );
	CI_ASSERT( status == noErr );

	// TODO: move to NodeAudioUnit method
	::AURenderCallbackStruct callbackStruct = { LineOutAudioUnit::renderCallback, &mRenderContext };
	status = ::AudioUnitSetProperty( audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, DeviceAudioUnit::Bus::Output, &callbackStruct, sizeof( callbackStruct ) );
	CI_ASSERT( status == noErr );

	mDevice->initialize();
	mInitialized = true;
}

void LineOutAudioUnit::uninitialize()
{
	mInitialized = false;
	mDevice->uninitialize();
}

void LineOutAudioUnit::start()
{
	mEnabled = true;
	mDevice->start();
}

void LineOutAudioUnit::stop()
{
	mEnabled = false;
	mDevice->stop();
}

DeviceRef LineOutAudioUnit::getDevice()
{
	return std::static_pointer_cast<Device>( mDevice );
}

::AudioUnit LineOutAudioUnit::getAudioUnit() const
{
	return mDevice->getComponentInstance();
}

OSStatus LineOutAudioUnit::renderCallback( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList )
{
	RenderContext *ctx = static_cast<NodeAudioUnit::RenderContext *>( data );
	LineOutAudioUnit *lineOut = static_cast<LineOutAudioUnit *>( ctx->node );

	lineOut->mInternalBuffer.zero(); // TODO: this might not be necessary, since it is done in process (sometimes)
	ctx->context->setCurrentTimeStamp( timeStamp );
	ctx->node->pullInputs( &lineOut->mInternalBuffer );
	copyToBufferList( bufferList, &lineOut->mInternalBuffer );

	checkBufferListNotClipping( bufferList, numFrames );
	return noErr;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - LineInAudioUnit
// ----------------------------------------------------------------------------------------------------

// FIXME: in a multi-graph situation, Path A is only pliable if device is used for both I/O in _this_ graph
//	- this is only checking if I and O are both in use
//	- only way I can think of to solve this is to keep a weak reference to Graph in both I and O units,
//	  check graph->output->device in initialize to see if it's the same

LineInAudioUnit::LineInAudioUnit( DeviceRef device, const Format &format )
: LineInNode( device, format ), mSynchroniousIO( false )
{
	mRenderBus = DeviceAudioUnit::Bus::Input; // TODO: remove, this shouldn't be necessary anymore

	mDevice = dynamic_pointer_cast<DeviceAudioUnit>( device );
	CI_ASSERT( mDevice );

	if( mChannelMode != ChannelMode::SPECIFIED ) {
		mChannelMode = ChannelMode::SPECIFIED;
		setNumChannels( 2 );
	}

}

LineInAudioUnit::~LineInAudioUnit()
{
	if( mAudioUnit ) {
		OSStatus status = ::AudioComponentInstanceDispose( mAudioUnit );
		CI_ASSERT( status == noErr );
	}
}

void LineInAudioUnit::initialize()
{
	mRenderContext.node = this;
	mRenderContext.context = dynamic_cast<ContextAudioUnit *>( getContext().get() );

	::AudioUnit audioUnit = getAudioUnit();
	CI_ASSERT( audioUnit );

	CI_ASSERT( ! mDevice->isInputConnected() );
	mDevice->setInputConnected();

	::AudioStreamBasicDescription asbd = cocoa::createFloatAsbd( getNumChannels(), getContext()->getSampleRate() );

	OSStatus status = ::AudioUnitSetProperty( audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, DeviceAudioUnit::Bus::Input, &asbd, sizeof( asbd ) );
	CI_ASSERT( status == noErr );

	if( mDevice->isOutputConnected() ) {
		LOG_V << "Synchronous I/O." << endl;
		// output node is expected to initialize device, since it is pulling to here.
		mSynchroniousIO = true;

		mBufferList = cocoa::createNonInterleavedBufferList( getNumChannels(), getContext()->getNumFramesPerBlock() );


		// TODO: move to NodeAudioUnit method
		::AURenderCallbackStruct callbackStruct = { LineInAudioUnit::inputCallback, &mRenderContext };
		status = ::AudioUnitSetProperty( audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callbackStruct, sizeof( callbackStruct ) );
		CI_ASSERT( status == noErr );
	}
	else {
		LOG_V << "ASynchronous I/O, initiate ringbuffer" << endl;

		mRingBuffer = unique_ptr<RingBuffer>( new RingBuffer( mDevice->getNumFramesPerBlock() * getNumChannels() ) );
		mBufferList = cocoa::createNonInterleavedBufferList( getNumChannels(), mDevice->getNumFramesPerBlock() );

		::AURenderCallbackStruct callbackStruct = { LineInAudioUnit::inputCallback, &mRenderContext };
		status = ::AudioUnitSetProperty( audioUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, DeviceAudioUnit::Bus::Input, &callbackStruct, sizeof( callbackStruct ) );
		CI_ASSERT( status == noErr );

		mDevice->initialize();
	}

	mInitialized = true;
}

void LineInAudioUnit::uninitialize()
{
	mInitialized = false;
	mDevice->uninitialize();
}

void LineInAudioUnit::start()
{
	if( ! mDevice->isOutputConnected() )
		mDevice->start();

	mEnabled = true;
	LOG_V << "started: " << mDevice->getName() << endl;
}

void LineInAudioUnit::stop()
{
	if( ! mDevice->isOutputConnected() )
		mDevice->stop();

	mEnabled = false;
	LOG_V << "stopped: " << mDevice->getName() << endl;
}

DeviceRef LineInAudioUnit::getDevice()
{
	return std::static_pointer_cast<Device>( mDevice );
}

::AudioUnit LineInAudioUnit::getAudioUnit() const
{
	return mDevice->getComponentInstance();
}

// TODO: do away with background thread logging
// - the right thing to do here is store an underrun framestamp
// - retrieved by getting Context::getCurrentSampleFrame(), UI can get last one in update() loop
void LineInAudioUnit::process( Buffer *buffer )
{
	if( mSynchroniousIO ) {

		mProcessBuffer = buffer;

		::AudioUnitRenderActionFlags flags = 0;
		const ::AudioTimeStamp *timeStamp = mRenderContext.context->getCurrentTimeStamp();
		OSStatus status = ::AudioUnitRender( getAudioUnit(), &flags, timeStamp, DeviceAudioUnit::Bus::Input, (UInt32)buffer->getNumFrames(), mBufferList.get() );
		CI_ASSERT( status == noErr );

		copyFromBufferList( buffer, mBufferList.get() );

		return;
	}

	// copy from async ringbuffer
	size_t numFrames = buffer->getNumFrames();
	for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ ) {
		size_t count = mRingBuffer->read( buffer->getChannel( ch ), numFrames );
		if( count != numFrames )
			LOG_V << " Warning, unexpected read count: " << count << ", expected: " << numFrames << " (ch = " << ch << ")" << endl;
	}
}

// TODO: this is duplicated code, move to NodeAudioUnit if it doesn't need to change
OSStatus LineInAudioUnit::renderCallback( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList )
{
	RenderContext *ctx = static_cast<NodeAudioUnit::RenderContext *>( data );
	LineInAudioUnit *lineIn = static_cast<LineInAudioUnit *>( ctx->node );

	copyToBufferList( bufferList, lineIn->mProcessBuffer );
	return noErr;
}

OSStatus LineInAudioUnit::inputCallback( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList )
{
	RenderContext *ctx = static_cast<NodeAudioUnit::RenderContext *>( data );
	LineInAudioUnit *lineIn = static_cast<LineInAudioUnit *>( ctx->node );
	CI_ASSERT( lineIn->mRingBuffer );
	
	::AudioBufferList *nodeBufferList = lineIn->mBufferList.get();
	OSStatus status = ::AudioUnitRender( lineIn->getAudioUnit(), flags, timeStamp, DeviceAudioUnit::Bus::Input, numFrames, nodeBufferList );
	CI_ASSERT( status == noErr );

	for( size_t ch = 0; ch < nodeBufferList->mNumberBuffers; ch++ ) {
		float *channel = static_cast<float *>( nodeBufferList->mBuffers[ch].mData );
		lineIn->mRingBuffer->write( channel, numFrames );
	}
	return status;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - EffectAudioUnit
// ----------------------------------------------------------------------------------------------------

EffectAudioUnit::EffectAudioUnit( UInt32 effectSubType, const Format &format )
: EffectNode( format ), mEffectSubType( effectSubType )
{
}

EffectAudioUnit::~EffectAudioUnit()
{
}

void EffectAudioUnit::initialize()
{
	Node::initialize(); // TEMP

	mRenderContext.node = this;
	mRenderContext.context = dynamic_cast<ContextAudioUnit *>( getContext().get() );

	::AudioComponentDescription comp{ 0 };
	comp.componentType = kAudioUnitType_Effect;
	comp.componentSubType = mEffectSubType;
	comp.componentManufacturer = kAudioUnitManufacturer_Apple;
	cocoa::findAndCreateAudioComponent( comp, &mAudioUnit );

	mBufferList = cocoa::createNonInterleavedBufferList( getNumChannels(), getContext()->getNumFramesPerBlock() );

	::AudioStreamBasicDescription asbd = cocoa::createFloatAsbd( getNumChannels(), getContext()->getSampleRate() );
	OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &asbd, sizeof( asbd ) );
	CI_ASSERT( status == noErr );
	status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &asbd, sizeof( asbd ) );
	CI_ASSERT( status == noErr );

	::AURenderCallbackStruct callbackStruct = { EffectAudioUnit::renderCallback, &mRenderContext };
	status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callbackStruct, sizeof( callbackStruct ) );
	CI_ASSERT( status == noErr );


	status = ::AudioUnitInitialize( mAudioUnit );
	CI_ASSERT( status == noErr );

	mInitialized = true;
}

void EffectAudioUnit::uninitialize()
{
	mInitialized = false;
	OSStatus status = ::AudioUnitUninitialize( mAudioUnit );
	CI_ASSERT( status == noErr );
}

// TODO: try pointing buffer list at processBuffer instead of copying
void EffectAudioUnit::process( Buffer *buffer )
{
	mProcessBuffer = buffer;

	::AudioUnitRenderActionFlags flags = 0;
	const ::AudioTimeStamp *timeStamp = mRenderContext.context->getCurrentTimeStamp();
	OSStatus status = ::AudioUnitRender( mAudioUnit, &flags, timeStamp, 0, (UInt32)buffer->getNumFrames(), mBufferList.get() );
	CI_ASSERT( status == noErr );

	copyFromBufferList( buffer, mBufferList.get() );
}

OSStatus EffectAudioUnit::renderCallback( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList )
{
	RenderContext *ctx = static_cast<NodeAudioUnit::RenderContext *>( data );
	EffectAudioUnit *effectNode = static_cast<EffectAudioUnit *>( ctx->node );

	copyToBufferList( bufferList, effectNode->mProcessBuffer );
	return noErr;
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
	mChannelMode = ChannelMode::MATCHES_OUTPUT;
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

	if( mInputs.size() > getNumBusses() ) {
		setMaxNumBusses( mInputs.size() );
	}

	for( UInt32 bus = 0; bus < mInputs.size(); bus++ ) {
		if( ! mInputs[bus] )
			continue;

		::AudioStreamBasicDescription busAsbd = cocoa::createFloatAsbd( mInputs[bus]->getNumChannels(), sampleRate );

		status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, bus, &busAsbd, sizeof( busAsbd ) );
		CI_ASSERT( status == noErr );

		float inputVolume = 1.0f;
		status = ::AudioUnitSetParameter( mAudioUnit, kMultiChannelMixerParam_Volume, kAudioUnitScope_Input, bus, inputVolume, 0 );
		CI_ASSERT( status == noErr );
	}

	status = ::AudioUnitInitialize( mAudioUnit );
	CI_ASSERT( status == noErr );

	mInitialized = true;
}

void MixerAudioUnit::uninitialize()
{
	mInitialized = false;
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
// MARK: - ContextAudioUnit
// ----------------------------------------------------------------------------------------------------

ContextRef ContextAudioUnit::createContext()
{
	return ContextRef( new ContextAudioUnit() );
}

LineOutNodeRef ContextAudioUnit::createLineOut( DeviceRef device, const Node::Format &format )
{
	return makeNode( new LineOutAudioUnit( device, format ) );
}

LineInNodeRef ContextAudioUnit::createLineIn( DeviceRef device, const Node::Format &format )
{
	return makeNode( new LineInAudioUnit( device, format ) );
}

MixerNodeRef ContextAudioUnit::createMixer( const Node::Format &format )
{
	return makeNode( new MixerAudioUnit( format ) );
}

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

	mInitialized = true;
	LOG_V << "graph initialize complete. samplerate: " << mSampleRate << ", frames per block: " << mNumFramesPerBlock << endl;
}

void ContextAudioUnit::initNode( NodeRef node )
{
	if( ! node )
		return;

//	node->setContext( shared_from_this() );

	// recurse through inputs
	for( NodeRef& inputNode : node->getInputs() )
		initNode( inputNode );

	node->initialize();
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
	for( auto &source : node->getInputs() )
		uninitNode( source );

	node->uninitialize();
}

} } // namespace audio2::cocoa