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

#include "audio2/GeneratorNode.h"
#include "audio2/RingBuffer.h"
#include "audio2/Debug.h"

#include "cinder/Utilities.h"

using namespace ci;
using namespace std;

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - GeneratorNode
// ----------------------------------------------------------------------------------------------------

GeneratorNode::GeneratorNode( const Format &format ) : Node( format )
{
	// GeneratorNode's don't have inputs, so disallow matches input channels
	if( mChannelMode == ChannelMode::MATCHES_INPUT )
		mChannelMode = ChannelMode::MATCHES_OUTPUT;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - BufferPlayerNode
// ----------------------------------------------------------------------------------------------------

BufferPlayerNode::BufferPlayerNode( const Format &format )
: PlayerNode( format )
{
}

BufferPlayerNode::BufferPlayerNode( BufferRef buffer, const Format &format )
: PlayerNode( format ), mBuffer( buffer )
{
	mNumFrames = mBuffer->getNumFrames();

	if( mNumChannelsUnspecified )
		setNumChannels( mBuffer->getNumChannels() );
}

void BufferPlayerNode::start()
{
	if( ! mBuffer ) {
		LOG_E << "no audio buffer, returning." << endl;
		return;
	}

	mReadPos = 0;
	mEnabled = true;

	LOG_V << "started" << endl;
}

void BufferPlayerNode::stop()
{
	mEnabled = false;

	LOG_V << "stopped" << endl;
}

// TODO: decide how best to allow this BufferPlayerNode to load audio files of a different format. options:
// a) use a converter that takes the passed in buffer and spits out a new buffer of the proper format
// b) change this BufferPlayerNode's format to match
//		- in the current system, requires an un-init, cleanup converters, re-init.
//		- if there are no converters and channels are 'runtime mapped' by the Context graph, this will probably work.
void BufferPlayerNode::setBuffer( BufferRef buffer )
{
	if( buffer->getNumChannels() != mNumChannels || buffer->getLayout() != mBufferLayout ) {
		LOG_E << "swapping in another BufferRef is currently limited to one with the same format as this BufferPlayerNode." << endl;
		return;
	}

	bool enabled = mEnabled;
	if( mEnabled )
		stop();

	mBuffer = buffer;
	mNumFrames = buffer->getNumFrames();

	if( enabled )
		start();
}

void BufferPlayerNode::process( Buffer *buffer )
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
// MARK: - FilePlayerNode
// ----------------------------------------------------------------------------------------------------


FilePlayerNode::FilePlayerNode( const Format &format )
: PlayerNode( format ), mNumFramesBuffered( 0 ), mSampleRate( 0 )
{
}

FilePlayerNode::~FilePlayerNode()
{
}

FilePlayerNode::FilePlayerNode( SourceFileRef sourceFile, bool isMultiThreaded, const Format &format )
: PlayerNode( format ), mSourceFile( sourceFile ), mMultiThreaded( isMultiThreaded ), mNumFramesBuffered( 0 ), mSampleRate( 0 )
{
	mNumFrames = mSourceFile->getNumFrames();
	mBufferFramesThreshold = mSourceFile->getNumFramesPerRead() / 2; // TODO: expose

	mFramesPerBlock = getContext()->getNumFramesPerBlock();
}

void FilePlayerNode::initialize()
{
	mSampleRate = getContext()->getSampleRate();
	mSourceFile->setNumChannels( getNumChannels() );
	mSourceFile->setSampleRate( mSampleRate );

	size_t paddingMultiplier = 2; // TODO: expose
	mReadBuffer = Buffer( getNumChannels(), mSourceFile->getNumFramesPerRead() );
	mRingBuffer = unique_ptr<RingBuffer>( new RingBuffer( getNumChannels() * mSourceFile->getNumFramesPerRead() * paddingMultiplier ) );

	if( mMultiThreaded ) {
		mReadOnBackground = true;
		mReadThread = unique_ptr<thread>( new thread( bind( &FilePlayerNode::readFromBackgroundThread, this ) ) );
	}
}

void FilePlayerNode::uninitialize()
{
	if( mMultiThreaded ) {
		mReadOnBackground = false;
		mReadThread->detach();
	}
}

void FilePlayerNode::setReadPosition( size_t pos )
{
	CI_ASSERT( mSourceFile );

	if( ! mMultiThreaded )
		mSourceFile->seek( pos );
	
	mReadPos = pos;
}

void FilePlayerNode::start()
{
	CI_ASSERT( mSourceFile );

	setReadPosition( 0 );
	mEnabled = true;

	LOG_V << "started" << endl;
}

void FilePlayerNode::stop()
{
	mEnabled = false;

	LOG_V << "stopped" << endl;
}

void FilePlayerNode::process( Buffer *buffer )
{
	size_t numFrames = buffer->getNumFrames();

	if( ! mMultiThreaded && moreFramesNeeded() )
		readFile();

	size_t readCount = std::min( mNumFramesBuffered, numFrames );

	for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ ) {
		size_t count = mRingBuffer->read( buffer->getChannel( ch ), readCount );
		CI_ASSERT( count == readCount );
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

bool FilePlayerNode::moreFramesNeeded()
{
	return ( mNumFramesBuffered < mBufferFramesThreshold && mReadPos < mNumFrames ) ? true : false;
}

void FilePlayerNode::readFromBackgroundThread()
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

void FilePlayerNode::readFile()
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

} // namespace audio2