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
#include "audio2/cocoa/CinderCoreAudio.h"
#include "audio2/audio.h"
#include "audio2/CinderAssert.h"
#include "audio2/Debug.h"

#include "cinder/Utilities.h"

#if defined( CINDER_MAC )
	#include "audio2/cocoa/DeviceManagerCoreAudio.h"
#else
	#include "audio2/cocoa/DeviceManagerAudioSession.h"
#endif

using namespace std;

namespace cinder { namespace audio2 { namespace cocoa {

enum DeviceBus { OUTPUT = 0, INPUT = 1 };

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeAudioUnit
// ----------------------------------------------------------------------------------------------------

NodeAudioUnit::~NodeAudioUnit()
{
	if( mAudioUnit && mOwnsAudioUnit ) {
		OSStatus status = ::AudioComponentInstanceDispose( mAudioUnit );
		CI_ASSERT( status == noErr );
	}
}

void NodeAudioUnit::initAu()
{
	OSStatus status = ::AudioUnitInitialize( mAudioUnit );
	CI_ASSERT( status == noErr );
}

void NodeAudioUnit::uninitAu()
{
	OSStatus status = ::AudioUnitUninitialize( mAudioUnit );
	CI_ASSERT( status == noErr );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeLineOutAudioUnit
// ----------------------------------------------------------------------------------------------------

NodeLineOutAudioUnit::NodeLineOutAudioUnit( DeviceRef device, const Format &format )
: NodeLineOut( device, format ), mProcessedFrames( 0 ), mSynchroniousIO( false )
{
	findAndCreateAudioComponent( getOutputAudioUnitDesc(), &mAudioUnit );
}

void NodeLineOutAudioUnit::initialize()
{
	// LineOut always needs an internal buffer to deliver to the ouput AU, so force one to be made.
	setProcessWithSumming();

	mRenderData.node = this;
	mRenderData.context = dynamic_cast<ContextAudioUnit *>( getContext().get() );

	::AudioStreamBasicDescription asbd = createFloatAsbd( getNumChannels(), getContext()->getSampleRate() );
	setAudioUnitProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, asbd, kAudioUnitScope_Input, DeviceBus::OUTPUT );

	UInt32 enableOutput = 1;
	setAudioUnitProperty( mAudioUnit, kAudioOutputUnitProperty_EnableIO, enableOutput, kAudioUnitScope_Output, DeviceBus::OUTPUT );

	UInt32 enableInput = mSynchroniousIO;
	setAudioUnitProperty( mAudioUnit, kAudioOutputUnitProperty_EnableIO, enableInput, kAudioUnitScope_Input, DeviceBus::INPUT );

	::AURenderCallbackStruct callbackStruct { NodeLineOutAudioUnit::renderCallback, &mRenderData };
	setAudioUnitProperty( mAudioUnit, kAudioUnitProperty_SetRenderCallback, callbackStruct, kAudioUnitScope_Input );

#if defined( CINDER_MAC )
	auto manager = dynamic_cast<DeviceManagerCoreAudio *>( Context::deviceManager() );
	CI_ASSERT( manager );

	manager->setCurrentDevice( mDevice->getKey(), mAudioUnit );
#endif

	initAu();
}

void NodeLineOutAudioUnit::uninitialize()
{
	uninitAu();
}

void NodeLineOutAudioUnit::start()
{
	if( mEnabled || ! mInitialized )
		return;

	mEnabled = true;
	OSStatus status = ::AudioOutputUnitStart( mAudioUnit );
	CI_ASSERT( status == noErr );

	LOG_V << "started." << endl;
}

void NodeLineOutAudioUnit::stop()
{
	if( ! mEnabled || ! mInitialized )
		return;

	mEnabled = false;
	OSStatus status = ::AudioOutputUnitStop( mAudioUnit );
	CI_ASSERT( status == noErr );

	LOG_V << "stopped: " << mDevice->getName() << endl;
}

OSStatus NodeLineOutAudioUnit::renderCallback( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList )
{
	RenderData *renderData = static_cast<NodeAudioUnit::RenderData *>( data );
	lock_guard<mutex> lock( renderData->context->getMutex() );

	// verify associated context still exists proceeding, which may not be true if we blocked in ~Context()
	if( ! renderData->node->getContext() )
		return noErr;

	NodeLineOutAudioUnit *lineOut = static_cast<NodeLineOutAudioUnit *>( renderData->node );
	lineOut->mInternalBuffer.zero();

	renderData->context->setCurrentTimeStamp( timeStamp );
	renderData->node->pullInputs( &lineOut->mInternalBuffer );
	copyToBufferList( bufferList, &lineOut->mInternalBuffer );

	lineOut->mProcessedFrames += lineOut->getFramesPerBlock();

	checkBufferListNotClipping( bufferList, numFrames );
	return noErr;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeLineInAudioUnit
// ----------------------------------------------------------------------------------------------------

NodeLineInAudioUnit::NodeLineInAudioUnit( DeviceRef device, const Format &format )
: NodeLineIn( device, format ), mSynchronousIO( false ), mLastUnderrun( 0 ), mLastOverrun( 0 )
{
	if( device->getNumOutputChannels() < mNumChannels )
		throw AudioFormatExc( "Device can not accomodate specified number of channels." );

	if( mChannelMode != ChannelMode::SPECIFIED ) {
		mChannelMode = ChannelMode::SPECIFIED;
		setNumChannels( 2 );
	}
}

NodeLineInAudioUnit::~NodeLineInAudioUnit()
{
}

void NodeLineInAudioUnit::initialize()
{
	mRenderData.node = this;
	mRenderData.context = dynamic_cast<ContextAudioUnit *>( getContext().get() );

	// see if synchronous I/O is possible by looking at the LineOut
	auto lineOutAu = dynamic_pointer_cast<NodeLineOutAudioUnit>( getContext()->getTarget() );

	if( lineOutAu ) {
		bool sameDevice = lineOutAu->getDevice() == mDevice;
		if( sameDevice ) {

			mSynchronousIO = true;
			mAudioUnit = lineOutAu->getAudioUnit();
			mOwnsAudioUnit = false;
		}
		else {
			// make our own AudioUnit, if we don't already have one (from a previous initialize())
			findAndCreateAudioComponent( getOutputAudioUnitDesc(), &mAudioUnit );

			mSynchronousIO = false;
			mOwnsAudioUnit = true;
		}
	}

	::AudioStreamBasicDescription asbd = createFloatAsbd( getNumChannels(), getContext()->getSampleRate() );
	setAudioUnitProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, asbd, kAudioUnitScope_Output, DeviceBus::INPUT );

	if( mSynchronousIO ) {
		LOG_V << "Synchronous I/O." << endl;
		// NodeLineOutAudioUnit is expected to initialize the AudioUnit, since it is pulling to here. But make sure input is enabled.
		if( ! lineOutAu->mSynchroniousIO ) {
			lineOutAu->mSynchroniousIO = true;
			if( lineOutAu->isInitialized() ) {
				bool lineOutEnabled = lineOutAu->isEnabled();
				lineOutAu->stop();
				lineOutAu->uninitialize();
				lineOutAu->initialize();
				lineOutAu->setEnabled( lineOutEnabled );
			}
		}

		mBufferList = createNonInterleavedBufferList( getNumChannels(), getContext()->getFramesPerBlock() );

		::AURenderCallbackStruct callbackStruct { NodeLineInAudioUnit::renderCallback, &mRenderData };
		setAudioUnitProperty( mAudioUnit, kAudioUnitProperty_SetRenderCallback, callbackStruct, kAudioUnitScope_Input );

#if defined( CINDER_COCOA_TOUCH )
		auto manager = dynamic_cast<DeviceManagerAudioSession *>( DeviceManager::instance() );
		CI_ASSERT( manager );

		manager->setInputEnabled();
#endif
	}
	else {
		LOG_V << "ASynchronous I/O, initiate ringbuffer" << endl;

		mRingBuffer = unique_ptr<RingBuffer>( new RingBuffer( mDevice->getFramesPerBlock() * getNumChannels() ) );
		mBufferList = createNonInterleavedBufferList( getNumChannels(), mDevice->getFramesPerBlock() );

		::AURenderCallbackStruct callbackStruct = { NodeLineInAudioUnit::inputCallback, &mRenderData };
		setAudioUnitProperty( mAudioUnit, kAudioOutputUnitProperty_SetInputCallback, callbackStruct, kAudioUnitScope_Global, DeviceBus::INPUT );

		UInt32 enableInput = 1;
		setAudioUnitProperty( mAudioUnit, kAudioOutputUnitProperty_EnableIO, enableInput, kAudioUnitScope_Input, DeviceBus::INPUT );

		UInt32 enableOutput = 0;
		setAudioUnitProperty( mAudioUnit, kAudioOutputUnitProperty_EnableIO, enableOutput, kAudioUnitScope_Output, DeviceBus::OUTPUT );

#if defined( CINDER_MAC )
		auto manager = dynamic_cast<DeviceManagerCoreAudio *>( Context::deviceManager() );
		CI_ASSERT( manager );

		manager->setCurrentDevice( mDevice->getKey(), mAudioUnit );
#endif
		initAu();
	}
}

// TODO: what about when synchronous IO and this guy is requested to uninit, does associated NodeLineOutAudioUnit need to be uninitialized too?
void NodeLineInAudioUnit::uninitialize()
{
	if( ! mSynchronousIO ) {
		uninitAu();
	}
}

void NodeLineInAudioUnit::start()
{
	if( mEnabled || ! mInitialized )
		return;

	mEnabled = true;

	if( ! mSynchronousIO ) {
		OSStatus status = ::AudioOutputUnitStart( mAudioUnit );
		CI_ASSERT( status == noErr );

		LOG_V << "started." << endl;
	}
}

void NodeLineInAudioUnit::stop()
{
	if( ! mEnabled || ! mInitialized )
		return;

	mEnabled = false;

	if( ! mSynchronousIO ) {
		OSStatus status = ::AudioOutputUnitStop( mAudioUnit );
		CI_ASSERT( status == noErr );
		LOG_V << "stopped: " << mDevice->getName() << endl;
	}
}

uint64_t NodeLineInAudioUnit::getLastUnderrun()
{
	uint64_t result = mLastUnderrun;
	mLastUnderrun = 0;
	return result;
}

uint64_t NodeLineInAudioUnit::getLastOverrun()
{
	uint64_t result = mLastOverrun;
	mLastOverrun = 0;
	return result;
}

void NodeLineInAudioUnit::process( Buffer *buffer )
{
	if( mSynchronousIO ) {
		mProcessBuffer = buffer;
		::AudioUnitRenderActionFlags flags = 0;
		const ::AudioTimeStamp *timeStamp = mRenderData.context->getCurrentTimeStamp();
		OSStatus status = ::AudioUnitRender( mAudioUnit, &flags, timeStamp, DeviceBus::INPUT, (UInt32)buffer->getNumFrames(), mBufferList.get() );
		CI_ASSERT( status == noErr );

		copyFromBufferList( buffer, mBufferList.get() );
	}
	else {
		// copy from ringbuffer
		size_t numFrames = buffer->getNumFrames();
		for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ ) {
			size_t count = mRingBuffer->read( buffer->getChannel( ch ), numFrames );
			if( count < numFrames )
				mLastUnderrun = getContext()->getNumProcessedFrames();
		}
	}
}

OSStatus NodeLineInAudioUnit::renderCallback( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList )
{
	RenderData *renderData = static_cast<NodeAudioUnit::RenderData *>( data );
	NodeLineInAudioUnit *lineIn = static_cast<NodeLineInAudioUnit *>( renderData->node );

	copyToBufferList( bufferList, lineIn->mProcessBuffer );
	return noErr;
}

OSStatus NodeLineInAudioUnit::inputCallback( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList )
{
	RenderData *renderData = static_cast<NodeAudioUnit::RenderData *>( data );
	NodeLineInAudioUnit *lineIn = static_cast<NodeLineInAudioUnit *>( renderData->node );
	CI_ASSERT( lineIn->mRingBuffer );
	
	::AudioBufferList *nodeBufferList = lineIn->mBufferList.get();
	OSStatus status = ::AudioUnitRender( lineIn->getAudioUnit(), flags, timeStamp, DeviceBus::INPUT, numFrames, nodeBufferList );
	CI_ASSERT( status == noErr );

	for( size_t ch = 0; ch < nodeBufferList->mNumberBuffers; ch++ ) {
		float *channel = static_cast<float *>( nodeBufferList->mBuffers[ch].mData );
		lineIn->mRingBuffer->write( channel, numFrames );
	}
	return status;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeEffectAudioUnit
// ----------------------------------------------------------------------------------------------------

NodeEffectAudioUnit::NodeEffectAudioUnit( UInt32 effectSubType, const Format &format )
: NodeEffect( format ), mEffectSubType( effectSubType )
{
}

NodeEffectAudioUnit::~NodeEffectAudioUnit()
{
}

void NodeEffectAudioUnit::initialize()
{
	mRenderData.node = this;
	mRenderData.context = dynamic_cast<ContextAudioUnit *>( getContext().get() );

	::AudioComponentDescription comp{ 0 };
	comp.componentType = kAudioUnitType_Effect;
	comp.componentSubType = mEffectSubType;
	comp.componentManufacturer = kAudioUnitManufacturer_Apple;
	findAndCreateAudioComponent( comp, &mAudioUnit );

	mBufferList = createNonInterleavedBufferList( getNumChannels(), getContext()->getFramesPerBlock() );

	::AudioStreamBasicDescription asbd = createFloatAsbd( getNumChannels(), getContext()->getSampleRate() );
	setAudioUnitProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, asbd, kAudioUnitScope_Input );
	setAudioUnitProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, asbd, kAudioUnitScope_Output );

	::AURenderCallbackStruct callbackStruct = { NodeEffectAudioUnit::renderCallback, &mRenderData };
	setAudioUnitProperty( mAudioUnit, kAudioUnitProperty_SetRenderCallback, callbackStruct, kAudioUnitScope_Input );

	initAu();
}

void NodeEffectAudioUnit::uninitialize()
{
	uninitAu();
}

// TODO: try pointing buffer list at processBuffer instead of copying
void NodeEffectAudioUnit::process( Buffer *buffer )
{
	mProcessBuffer = buffer;

	::AudioUnitRenderActionFlags flags = 0;
	const ::AudioTimeStamp *timeStamp = mRenderData.context->getCurrentTimeStamp();
	OSStatus status = ::AudioUnitRender( mAudioUnit, &flags, timeStamp, 0, (UInt32)buffer->getNumFrames(), mBufferList.get() );
	CI_ASSERT( status == noErr );

	copyFromBufferList( buffer, mBufferList.get() );
}

OSStatus NodeEffectAudioUnit::renderCallback( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList )
{
	RenderData *renderData = static_cast<NodeAudioUnit::RenderData *>( data );
	NodeEffectAudioUnit *effectNode = static_cast<NodeEffectAudioUnit *>( renderData->node );

	copyToBufferList( bufferList, effectNode->mProcessBuffer );
	return noErr;
}

void NodeEffectAudioUnit::setParameter( ::AudioUnitParameterID paramId, float val )
{
	setAudioUnitParam( mAudioUnit, paramId, val, kAudioUnitScope_Global );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - ContextAudioUnit
// ----------------------------------------------------------------------------------------------------

NodeLineOutRef ContextAudioUnit::createLineOut( DeviceRef device, const NodeTarget::Format &format )
{
	return makeNode( new NodeLineOutAudioUnit( device, format ) );
}

NodeLineInRef ContextAudioUnit::createLineIn( DeviceRef device, const Node::Format &format )
{
	return makeNode( new NodeLineInAudioUnit( device, format ) );
}

ContextAudioUnit::~ContextAudioUnit()
{
}

} } } // namespace cinder::audio2::cocoa