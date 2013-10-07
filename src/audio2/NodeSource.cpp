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
#include "audio2/audio.h"
#include "audio2/RingBuffer.h"
#include "audio2/Debug.h"

#include "cinder/Utilities.h"

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

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeLineIn
// ----------------------------------------------------------------------------------------------------

NodeLineIn::NodeLineIn( const DeviceRef &device, const Format &format )
: NodeSource( format ), mDevice( device )
{
	if( boost::indeterminate( format.getAutoEnable() ) )
		setAutoEnabled();
}

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeBufferPlayer
// ----------------------------------------------------------------------------------------------------

NodeBufferPlayer::NodeBufferPlayer( const Format &format )
: NodeSamplePlayer( format )
{
	// If user didn't set a specified channel count, set to one until further notice.
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

void NodeBufferPlayer::setBuffer( const BufferRef &buffer )
{
	lock_guard<mutex> lock( getContext()->getMutex() );

	bool enabled = mEnabled;
	if( mEnabled )
		stop();

	if( buffer->getNumChannels() != mBuffer->getNumChannels() ) {
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
: NodeSamplePlayer( format ), mNumFramesBuffered( 0 ), mSampleRate( 0 )
{
}

NodeFilePlayer::~NodeFilePlayer()
{
	if( mMultiThreaded ) {
		mReadOnBackground = false;
		mReadThread->detach(); // FIXME: this is probably unsafe
	}
}

NodeFilePlayer::NodeFilePlayer( const SourceFileRef &sourceFile, bool isMultiThreaded, const Format &format )
: NodeSamplePlayer( format ), mSourceFile( sourceFile ), mMultiThreaded( isMultiThreaded ), mNumFramesBuffered( 0 ), mSampleRate( 0 )
{
	mNumFrames = mSourceFile->getNumFrames();
	mBufferFramesThreshold = mSourceFile->getNumFramesPerRead() / 2; // TODO: expose

	mFramesPerBlock = getContext()->getFramesPerBlock();
}

void NodeFilePlayer::initialize()
{
	mSampleRate = getContext()->getSampleRate();
	mSourceFile->setNumChannels( getNumChannels() );
	mSourceFile->setSampleRate( mSampleRate );

	size_t paddingMultiplier = 2; // TODO: expose
	mReadBuffer = Buffer( mSourceFile->getNumFramesPerRead(), getNumChannels() );
	mRingBuffer = unique_ptr<RingBuffer>( new RingBuffer( getNumChannels() * mSourceFile->getNumFramesPerRead() * paddingMultiplier ) );

	if( mMultiThreaded ) {
		mReadOnBackground = true;
		mReadThread = unique_ptr<thread>( new thread( bind( &NodeFilePlayer::readFromBackgroundThread, this ) ) );
	}
}

void NodeFilePlayer::setReadPosition( size_t pos )
{
	CI_ASSERT( mSourceFile );

	if( ! mMultiThreaded )
		mSourceFile->seek( pos );
	
	mReadPos = pos;
}

void NodeFilePlayer::start()
{
	CI_ASSERT( mSourceFile );

	setReadPosition( 0 );
	mEnabled = true;

	LOG_V << "started" << endl;
}

void NodeFilePlayer::stop()
{
	mEnabled = false;

	LOG_V << "stopped" << endl;
}

void NodeFilePlayer::process( Buffer *buffer )
{
	size_t numFrames = buffer->getNumFrames();

	if( ! mMultiThreaded && moreFramesNeeded() )
		readFile();

	size_t readCount = std::min( mNumFramesBuffered, numFrames );

	for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ ) {
		CI_ASSERT( mRingBuffer->getAvailableRead() >= readCount );
		mRingBuffer->read( buffer->getChannel( ch ), readCount );
	}
	mNumFramesBuffered -= readCount;

	// check if end of file
	if( readCount < numFrames ) {
		buffer->zero( readCount, numFrames - readCount );

		if( mReadPos >= mNumFrames ) {
			if( mLoop ) {
				setReadPosition( 0 );
				return;
			}

			mEnabled = false;
		}
//		else
//			LOG_V << "BUFFER UNDERRUN" << endl;
	}
}

bool NodeFilePlayer::moreFramesNeeded()
{
	return ( mNumFramesBuffered < mBufferFramesThreshold && mReadPos < mNumFrames ) ? true : false;
}

void NodeFilePlayer::readFromBackgroundThread()
{
	size_t readMilliseconds = ( 1000 * mSourceFile->getNumFramesPerRead() ) / mSampleRate;
	size_t lastReadPos = mReadPos;
	while( mReadOnBackground ) {
		if( ! moreFramesNeeded() ) {
			// FIXME: this is still causing underruns. need either:
			// a) a higher resolution timer
			// b) condition + mutex
			ci::sleep( readMilliseconds / 2 );
			continue;
		}

		size_t readPos = mReadPos;
		if( readPos != lastReadPos )
			mSourceFile->seek( readPos );

		readFile();
		lastReadPos = mReadPos;
	}
}

// FIXME: this copy is really janky
// - mReadBuffer and mRingBuffer are much bigger than the block size
// - since they are non-interleaved, need to pack them in sections = numFramesPerBlock so stereo channels can be
//   pulled out appropriately.
// - Ideally, there would only be one buffer copied to on the background thread and then one copy/consume in process()	

void NodeFilePlayer::readFile()
{
	size_t numRead = mSourceFile->read( &mReadBuffer );
	mReadPos += numRead;

	size_t numFramesPerBlock = mFramesPerBlock;
	size_t channelOffset = 0;
	while( numRead ) {
		CI_ASSERT( numRead <= mNumFrames );

		size_t writeCount = std::min( numFramesPerBlock, numRead );
		for( size_t ch = 0; ch < mReadBuffer.getNumChannels(); ch++ )
			mRingBuffer->write( mReadBuffer.getChannel( ch ) + channelOffset, writeCount );

		channelOffset += writeCount;

		numRead -= writeCount;
		mNumFramesBuffered += writeCount;
	}
}

} } // namespace cinder::audio2