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

#include "audio2/NodeSource.h"
#include "audio2/Context.h"
#include "audio2/Debug.h"

#include "cinder/Utilities.h"
#include "cinder/CinderMath.h"

using namespace ci;
using namespace std;

namespace cinder { namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeSource
// ----------------------------------------------------------------------------------------------------

NodeSource::NodeSource( const Format &format ) : Node( format )
{
	mInputs.clear();

	// NodeSource's don't have inputs, so disallow matches input channels
	if( mChannelMode == ChannelMode::MATCHES_INPUT )
		mChannelMode = ChannelMode::MATCHES_OUTPUT;
}

NodeSource::~NodeSource()
{
}

void NodeSource::connectInput( const NodeRef &input, size_t bus )
{
	CI_ASSERT_MSG( 0, "NodeSource does not support inputs" );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeLineIn
// ----------------------------------------------------------------------------------------------------

NodeLineIn::NodeLineIn( const DeviceRef &device, const Format &format )
: NodeSource( format ), mDevice( device )
{
	if( boost::indeterminate( format.getAutoEnable() ) )
		setAutoEnabled();
}

NodeLineIn::~NodeLineIn()
{
}

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeSamplePlayer
// ----------------------------------------------------------------------------------------------------

void NodeSamplePlayer::seekToTime( double readPositionSeconds )
{
	return seek( size_t( readPositionSeconds * (double)getContext()->getSampleRate() ) );
}

double NodeSamplePlayer::getReadPositionTime() const
{
	return (double)mReadPos / (double)getContext()->getSampleRate();
}

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeBufferPlayer
// ----------------------------------------------------------------------------------------------------

NodeBufferPlayer::NodeBufferPlayer( const Format &format )
: NodeSamplePlayer( format )
{
	// If the Format doesn't specify num channels, set to one until further notice.
	if( ! mChannelMode == ChannelMode::SPECIFIED ) {
		mChannelMode = ChannelMode::SPECIFIED;
		mNumChannels = mBuffer->getNumChannels();
	}
}

NodeBufferPlayer::NodeBufferPlayer( const BufferRef &buffer, const Format &format )
: NodeSamplePlayer( format ), mBuffer( buffer )
{
	mNumFrames = mBuffer->getNumFrames();

	// force channel mode to match buffer
	mChannelMode = ChannelMode::SPECIFIED;
	setNumChannels( mBuffer->getNumChannels() );
}

void NodeBufferPlayer::start()
{
	if( ! mBuffer ) {
		LOG_E << "no audio buffer, returning." << endl;
		return;
	}

	mReadPos = 0;
	mEnabled = true;

	LOG_V << "started" << endl;
}

void NodeBufferPlayer::stop()
{
	mEnabled = false;

	LOG_V << "stopped" << endl;
}

void NodeBufferPlayer::seek( size_t readPositionFrames )
{
	mReadPos = math<size_t>::clamp( readPositionFrames, 0, mNumFrames );
}

void NodeBufferPlayer::setBuffer( const BufferRef &buffer )
{
	lock_guard<mutex> lock( getContext()->getMutex() );

	bool enabled = mEnabled;
	if( mEnabled )
		stop();

	if( mNumChannels != buffer->getNumChannels() ) {
		setNumChannels( buffer->getNumChannels() );
		configureConnections();
	}

	mBuffer = buffer;
	mNumFrames = buffer->getNumFrames();

	if( enabled )
		start();
}

void NodeBufferPlayer::process( Buffer *buffer )
{
	size_t readPos = mReadPos;
	size_t numFrames = buffer->getNumFrames();
	size_t readCount = std::min( mNumFrames - readPos, numFrames );

	for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ )
		memcpy( buffer->getChannel( ch ), &mBuffer->getChannel( ch )[readPos], readCount * sizeof( float ) );

	if( readCount < numFrames  ) {
		buffer->zero( readCount, numFrames - readCount );

		if( mLoop ) {
			mReadPos = 0;
			return;
		} else
			mEnabled = false;
	}

	mReadPos += readCount;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeFilePlayer
// ----------------------------------------------------------------------------------------------------

NodeFilePlayer::NodeFilePlayer( const Format &format )
: NodeSamplePlayer( format ), mRingBufferPaddingFactor( 2 )
{
	// force channel mode to match buffer
	mChannelMode = ChannelMode::SPECIFIED;
}

NodeFilePlayer::NodeFilePlayer( const SourceFileRef &sourceFile, bool isMultiThreaded, const Format &format )
: NodeSamplePlayer( format ), mSourceFile( sourceFile ), mMultiThreaded( isMultiThreaded ), mRingBufferPaddingFactor( 2 )
{
	// force channel mode to match buffer
	mChannelMode = ChannelMode::SPECIFIED;
	setNumChannels( mSourceFile->getNumChannels() );
	mNumFrames = mSourceFile->getNumFrames();
}

NodeFilePlayer::~NodeFilePlayer()
{
}

void NodeFilePlayer::initialize()
{
	mIoBuffer.setSize( mSourceFile->getMaxFramesPerRead(), mNumChannels );

	for( size_t i = 0; i < mNumChannels; i++ )
		mRingBuffers.emplace_back( mSourceFile->getMaxFramesPerRead() * mRingBufferPaddingFactor );

	mBufferFramesThreshold = mRingBuffers[0].getSize() / 2;

	if( mMultiThreaded ) {
		mReadOnBackground = true;
		mReadThread = unique_ptr<thread>( new thread( bind( &NodeFilePlayer::readFromBackgroundThread, this ) ) );
	}

	LOG_V << " multithreaded: " << boolalpha << mMultiThreaded << dec << ", ringbufer frames: " << mRingBuffers[0].getSize() << ", mBufferFramesThreshold: " << mBufferFramesThreshold << ", source file max frames per read: " << mSourceFile->getMaxFramesPerRead() << endl;
}

void NodeFilePlayer::uninitialize()
{
	destroyIoThread();
}

void NodeFilePlayer::start()
{
	if( ! mSourceFile ) {
		LOG_E << "no source file, returning." << endl;
		return;
	}

	seek( 0 );
	mEnabled = true;

	LOG_V << "started" << endl;
}

void NodeFilePlayer::stop()
{
	mEnabled = false;

	LOG_V << "stopped" << endl;
}

void NodeFilePlayer::seek( size_t readPositionFrames )
{
	if( ! mSourceFile ) {
		LOG_E << "no source file, returning." << endl;
		return;
	}

	unique_lock<mutex> lock( mIoMutex );

	mReadPos = math<size_t>::clamp( readPositionFrames, 0, mNumFrames );
	mSourceFile->seekToTime( mReadPos );
}

void NodeFilePlayer::setSourceFile( const SourceFileRef &sourceFile )
{
	lock_guard<mutex> lock( getContext()->getMutex() );

	bool enabled = mEnabled;
	if( mEnabled )
		stop();

	if( mNumChannels != sourceFile->getNumChannels() ) {
		setNumChannels( sourceFile->getNumChannels() );
		configureConnections();
	}

	mSourceFile = sourceFile;
	mNumFrames = sourceFile->getNumFrames();

	if( enabled )
		start();
}

uint64_t NodeFilePlayer::getLastUnderrun()
{
	uint64_t result = mLastUnderrun;
	mLastUnderrun = 0;
	return result;
}

uint64_t NodeFilePlayer::getLastOverrun()
{
	uint64_t result = mLastOverrun;
	mLastOverrun = 0;
	return result;
}

void NodeFilePlayer::process( Buffer *buffer )
{
	size_t numFrames = buffer->getNumFrames();
	size_t readPos = mReadPos;
	size_t numReadAvail = mRingBuffers[0].getAvailableRead();

//	LOG_V << "numReadAvail: " << numReadAvail << endl;

	if( numReadAvail < mBufferFramesThreshold ) {
		if( mMultiThreaded )
			mNeedMoreSamplesCond.notify_one();
		else
			readFile();
	}

	size_t readCount = std::min( numReadAvail, numFrames );

	for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ ) {
		if( ! mRingBuffers[ch].read( buffer->getChannel( ch ), readCount ) )
			mLastUnderrun = getContext()->getNumProcessedFrames();
	}

	// zero any unused frames
	if( readCount < numFrames ) {
		buffer->zero( readCount, numFrames - readCount );

		// check if end of file
		if( readPos + readCount >= mNumFrames ) {
			if( mLoop ) {
				seek( 0 );
				return;
			}

			mEnabled = false;
		}
	}
}

void NodeFilePlayer::readFromBackgroundThread()
{
	size_t lastReadPos = mReadPos;
	while( true ) {
		unique_lock<mutex> lock( mIoMutex );
		mNeedMoreSamplesCond.wait( lock );

		if( ! mReadOnBackground )
			return;

		size_t readPos = mReadPos;
		if( readPos != lastReadPos )
			mSourceFile->seek( readPos );

		readFile();
		lastReadPos = mReadPos;
	}
}

void NodeFilePlayer::readFile()
{
	size_t availableWrite = mRingBuffers[0].getAvailableWrite();
	size_t numFramesToRead = min( availableWrite, mNumFrames - mReadPos );

	if( ! numFramesToRead ) {
		mLastOverrun = getContext()->getNumProcessedFrames();
		return;
	}

	mIoBuffer.setNumFrames( numFramesToRead );

	size_t numRead = mSourceFile->read( &mIoBuffer );
	mReadPos += numRead;

	for( size_t ch = 0; ch < mNumChannels; ch++ ) {
		if( ! mRingBuffers[ch].write( mIoBuffer.getChannel( ch ), numRead ) ) {
			mLastOverrun = getContext()->getNumProcessedFrames();
			return;
		}
	}

//	LOG_V << "availableWrite: " << availableWrite << ", numFramesToRead: " << numFramesToRead << ", numRead: " << numRead << endl;
}

void NodeFilePlayer::destroyIoThread()
{
	if( mMultiThreaded && mReadThread ) {
		LOG_V << "destroying I/O thread" << endl;
		mReadOnBackground = false;
		mNeedMoreSamplesCond.notify_one();
		mReadThread->join();
	}
}

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeCallbackProcessor
// ----------------------------------------------------------------------------------------------------

void NodeCallbackProcessor::process( Buffer *buffer )
{
	if( mCallbackFn )
		mCallbackFn( buffer, getContext()->getSampleRate() );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeGen's
// ----------------------------------------------------------------------------------------------------

NodeGen::NodeGen( const Format &format )
	: NodeSource( format )
{
	mChannelMode = ChannelMode::SPECIFIED;
	setNumChannels( 1 );
}

void NodeGen::initialize()
{
	mSampleRate = (float)getContext()->getSampleRate();
	mFreq.initialize( getContext() );
}

void NodeGenNoise::process( Buffer *buffer )
{
	float *data = buffer->getData();
	size_t count = buffer->getSize();

	for( size_t i = 0; i < count; i++ )
		data[i] = ci::randFloat( -1.0f, 1.0f );
}

void NodeGenSine::process( Buffer *buffer )
{
	float *data = buffer->getData();
	const size_t count = buffer->getSize();
	const float phaseMul = float( 2.0 * M_PI / (double)mSampleRate );
	float phase = mPhase;

	if( mFreq.isVaryingNextEval() ) {
		float *freqValues = mFreq.getValueArray();
		for( size_t i = 0; i < count; i++ ) {
			data[i] = math<float>::sin( phase );
			phase = fmodf( phase + freqValues[i] * phaseMul, M_PI * 2.0 );
		}
	}
	else {
		const float phaseIncr = mFreq.getValue() * phaseMul;
		for( size_t i = 0; i < count; i++ ) {
			data[i] = math<float>::sin( phase );
			phase = fmodf( phase + phaseIncr, M_PI * 2.0 );
		}
	}

	mPhase = phase;
}

void NodeGenPhasor::process( Buffer *buffer )
{
	float *data = buffer->getData();
	const size_t count = buffer->getSize();
	const float phaseMul = 1.0f / mSampleRate;
	float phase = mPhase;

	if( mFreq.isVaryingNextEval() ) {
		float *freqValues = mFreq.getValueArray();
		for( size_t i = 0; i < count; i++ ) {
			data[i] = phase;
			phase = fmodf( phase + freqValues[i] * phaseMul, 1.0f );
		}
	}
	else {
		const float phaseIncr = mFreq.getValue() * phaseMul;
		for( size_t i = 0; i < count; i++ ) {
			data[i] = phase;
			phase = fmodf( phase + phaseIncr, 1.0f );
		}
	}

	mPhase = phase;
}

void NodeGenTriangle::process( Buffer *buffer )
{
	float phaseIncr = mFreq.getValue() / mSampleRate;
	float *data = buffer->getData();
	size_t count = buffer->getSize();

	for( size_t i = 0; i < count; i++ )	{

		float phase = fmodf( mPhase, 1.0f );
		mPhase = phase + phaseIncr;

		float up = phase * mUpSlope;
		float down = (-phase + 1 ) * mDownSlope;

		data[i] = std::min( up, down );
	}
}

} } // namespace cinder::audio2