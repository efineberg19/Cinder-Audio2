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

#include "cinder/audio2/SamplePlayer.h"
#include "cinder/audio2/Context.h"
#include "cinder/audio2/Debug.h"
#include "cinder/CinderMath.h"

using namespace ci;
using namespace std;

namespace cinder { namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - SamplePlayer
// ----------------------------------------------------------------------------------------------------

SamplePlayer::SamplePlayer( const Format &format )
: NodeInput( format ), mNumFrames( 0 ), mReadPos( 0 ), mLoop( false ), mStartAtBeginning( true )
{
}

void SamplePlayer::seekToTime( double readPositionSeconds )
{
	return seek( size_t( readPositionSeconds * (double)getContext()->getSampleRate() ) );
}

double SamplePlayer::getReadPositionTime() const
{
	return (double)mReadPos / (double)getContext()->getSampleRate();
}

// ----------------------------------------------------------------------------------------------------
// MARK: - BufferPlayer
// ----------------------------------------------------------------------------------------------------

BufferPlayer::BufferPlayer( const Format &format )
: SamplePlayer( format )
{
}

BufferPlayer::BufferPlayer( const BufferRef &buffer, const Format &format )
: SamplePlayer( format ), mBuffer( buffer )
{
	mNumFrames = mBuffer->getNumFrames();

	// force channel mode to match buffer
	mChannelMode = ChannelMode::SPECIFIED;
	setNumChannels( mBuffer->getNumChannels() );
}

void BufferPlayer::start()
{
	if( ! mBuffer ) {
		LOG_E( "no audio buffer, returning." );
		return;
	}

	if( mStartAtBeginning )
		mReadPos = 0;

	mEnabled = true;

	LOG_V( "started" );
}

void BufferPlayer::stop()
{
	mEnabled = false;

	LOG_V( "stopped" );
}

void BufferPlayer::seek( size_t readPositionFrames )
{
	mReadPos = math<size_t>::clamp( readPositionFrames, 0, mNumFrames );
}

void BufferPlayer::setBuffer( const BufferRef &buffer )
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

void BufferPlayer::loadBuffer( const SourceFileRef &sourceFile )
{
	auto sf = sourceFile->clone();

	sf->setOutputFormat( getContext()->getSampleRate() );
	setBuffer( sf->loadBuffer() );
}

void BufferPlayer::process( Buffer *buffer )
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
// MARK: - FilePlayer
// ----------------------------------------------------------------------------------------------------

FilePlayer::FilePlayer( const Format &format )
: SamplePlayer( format ), mRingBufferPaddingFactor( 2 )
{
	// force channel mode to match buffer
	mChannelMode = ChannelMode::SPECIFIED;
}

FilePlayer::FilePlayer( const SourceFileRef &sourceFile, bool isReadAsync, const Format &format )
: SamplePlayer( format ), mSourceFile( sourceFile ), mIsReadAsync( isReadAsync ), mRingBufferPaddingFactor( 2 )
{
	// force channel mode to match buffer
	mChannelMode = ChannelMode::SPECIFIED;
	setNumChannels( mSourceFile->getNumChannels() );
	mNumFrames = 0; // will be updated once SourceFile's output samplerate is set
}

FilePlayer::~FilePlayer()
{
	if( mInitialized )
		destroyReadThreadImpl();
}

void FilePlayer::initialize()
{
	if( mSourceFile ) {
		mSourceFile->setOutputFormat( getContext()->getSampleRate() );
		mNumFrames = mSourceFile->getNumFrames();
	}

	mIoBuffer.setSize( mSourceFile->getMaxFramesPerRead(), mNumChannels );

	for( size_t i = 0; i < mNumChannels; i++ )
		mRingBuffers.emplace_back( mSourceFile->getMaxFramesPerRead() * mRingBufferPaddingFactor );

	mBufferFramesThreshold = mRingBuffers[0].getSize() / 2;

	if( mIsReadAsync ) {
		mAsyncReadShouldQuit = false;
		mReadThread = unique_ptr<thread>( new thread( bind( &FilePlayer::readAsyncImpl, this ) ) );
	}

	LOG_V( " multithreaded: " << boolalpha << mIsReadAsync << dec << ", ringbufer frames: " << mRingBuffers[0].getSize() << ", mBufferFramesThreshold: " << mBufferFramesThreshold << ", source file max frames per read: " << mSourceFile->getMaxFramesPerRead() );
}

void FilePlayer::uninitialize()
{
	destroyReadThreadImpl();
}

void FilePlayer::start()
{
	if( mEnabled || ! mSourceFile ) {
		LOG_E( "no source file, returning." );
		return;
	}

	if( mStartAtBeginning )
		seekImpl( 0 );

	mEnabled = true;

	LOG_V( "started" );
}

void FilePlayer::stop()
{
	mEnabled = false;

	LOG_V( "stopped" );
}

void FilePlayer::seek( size_t readPositionFrames )
{
	if( ! mSourceFile ) {
		LOG_E( "no source file, returning." );
		return;
	}

	// Synchronize with the mutex that protects the read thread, which is different depending on if
	// read is async or sync (done on audio thread)
	mutex &m = mIsReadAsync ? mAsyncReadMutex : getContext()->getMutex();
	lock_guard<mutex> lock( m );

	seekImpl( readPositionFrames );
}

void FilePlayer::setSourceFile( const SourceFileRef &sourceFile )
{
	// update source's samplerate to match context
	sourceFile->setOutputFormat( getContext()->getSampleRate(), sourceFile->getNumChannels() );

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

uint64_t FilePlayer::getLastUnderrun()
{
	uint64_t result = mLastUnderrun;
	mLastUnderrun = 0;
	return result;
}

uint64_t FilePlayer::getLastOverrun()
{
	uint64_t result = mLastOverrun;
	mLastOverrun = 0;
	return result;
}

void FilePlayer::process( Buffer *buffer )
{
	size_t numFrames = buffer->getNumFrames();
	size_t readPos = mReadPos;
	size_t numReadAvail = mRingBuffers[0].getAvailableRead();

	if( numReadAvail < mBufferFramesThreshold ) {
		if( mIsReadAsync )
			mIssueAsyncReadCond.notify_one();
		else
			readImpl();
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
				// TODO: instead of zeroing above, should fill with samples from the beginning of file
				// - these should also already be in the ringbuffer, since a seek is done there as well. Rethink this path.
				seekImpl( 0 );
			}
			else
				mEnabled = false;
		}
	}
}

void FilePlayer::readAsyncImpl()
{
	size_t lastReadPos = mReadPos;
	while( true ) {
		unique_lock<mutex> lock( mAsyncReadMutex );
		mIssueAsyncReadCond.wait( lock );

		if( mAsyncReadShouldQuit )
			return;

		size_t readPos = mReadPos;
		if( readPos != lastReadPos )
			mSourceFile->seek( readPos );

		readImpl();
		lastReadPos = mReadPos;
	}
}

void FilePlayer::readImpl()
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
}

void FilePlayer::seekImpl( size_t readPos )
{
	mReadPos = math<size_t>::clamp( readPos, 0, mNumFrames );
	mSourceFile->seek( mReadPos );
}

void FilePlayer::destroyReadThreadImpl()
{
	if( mIsReadAsync && mReadThread ) {
		LOG_V( "destroying I/O thread" );
		mAsyncReadShouldQuit = true;
		mIssueAsyncReadCond.notify_one();
		mReadThread->join();
	}
}

} } // namespace cinder::audio2