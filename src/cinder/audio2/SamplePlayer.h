/*
 Copyright (c) 2014, The Cinder Project

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

#pragma once

#include "cinder/audio2/NodeInput.h"
#include "cinder/audio2/Source.h"
#include "cinder/audio2/dsp/RingBuffer.h"

#include <thread>
#include <mutex>
#include <condition_variable>

namespace cinder { namespace audio2 {

typedef std::shared_ptr<class SamplePlayer>				SamplePlayerRef;
typedef std::shared_ptr<class BufferPlayer>				BufferPlayerRef;
typedef std::shared_ptr<class FilePlayer>				FilePlayerRef;

//! \brief Base Node class for sampled audio playback. Can do operations like seek and loop.
//! \note SamplePlayer itself doesn't process any audio, but contains the common interface for Node's that do.
//! \see BufferPlayer, FilePlayer
class SamplePlayer : public NodeInput {
public:
	virtual ~SamplePlayer() {}

	//! Seek the read position to \a readPositionFrames
	virtual void seek( size_t readPositionFrames ) = 0;

	//! Seek to read position \a readPositionSeconds
	void seekToTime( double readPositionSeconds );
	//! Gets the current read position in frames
	size_t getReadPosition() const	{ return mReadPos; }
	//! Gets the current read position in seconds.
	double getReadPositionTime() const;
	//! Returns whether the SamplePlayer has reached EOF (end of file). If true, isEnabled() will also return false.
	bool isEof() const				{ return mIsEof; }

	//! Sets whether playing continues from beginning after the end is reached (default = false)
	void setLoop( bool b = true )	{ mLoop = b; }
	//! Gets whether playing continues from beginning after the end is reached (default = false)
	bool getLoop() const			{ return mLoop; }
	//! Sets whether start() resets the read position to zero (default = true).
	void setStartAtBeginning( bool b = true )	{ mStartAtBeginning = b; }
	//! Gets whether start() resets the read position to zero (default = true).
	bool getStartAtBeginning() const			{ return mStartAtBeginning; }
	//! Returns the total number of frames this SamplePlayer will play from beginning to end.
	size_t getNumFrames() const	{ return mNumFrames; }

protected:
	SamplePlayer( const Format &format = Format() );

	size_t mNumFrames;
	std::atomic<size_t> mReadPos;
	std::atomic<bool>	mLoop, mIsEof;
	bool				mStartAtBeginning;
};

//! Buffer-based sample player. In other words, all samples are loaded into memory before playback.
class BufferPlayer : public SamplePlayer {
public:
	//! Constructs a BufferPlayer without a buffer, with the assumption one will be set later. \note Format::channels() can still be used to allocate the expected channel count ahead of time.
	BufferPlayer( const Format &format = Format() );
	//! Constructs a BufferPlayer with \a buffer. \note Channel mode is always ChannelMode::SPECIFIED and num channels matches \a buffer. Format::channels() is ignored.
	BufferPlayer( const BufferRef &buffer, const Format &format = Format() );

	virtual ~BufferPlayer() {}

	virtual void start() override;
	virtual void stop() override;
	virtual void seek( size_t readPositionFrames ) override;
	virtual void process( Buffer *buffer ) override;

	//! Loads and stores a reference to a Buffer created from the entire contents of \a sourceFile.
	void loadBuffer( const SourceFileRef &sourceFile );

	void setBuffer( const BufferRef &buffer );
	const BufferRef& getBuffer() const	{ return mBuffer; }

protected:
	BufferRef mBuffer;
};

class FilePlayer : public SamplePlayer {
public:
	FilePlayer( const Format &format = Format() );
	//! \note \a sourceFile's samplerate is forced to match this Node's Context.
	FilePlayer( const SourceFileRef &sourceFile, bool isReadAsync = true, const Format &format = Node::Format() );
	virtual ~FilePlayer();

	void initialize() override;
	void uninitialize() override;

	virtual void start() override;
	virtual void stop() override;
	virtual void seek( size_t readPositionFrames ) override;
	virtual void process( Buffer *buffer ) override;

	bool isReadAsync() const	{ return mIsReadAsync; }

	//! \note \a sourceFile's samplerate is forced to match this Node's Context.
	void setSourceFile( const SourceFileRef &sourceFile );
	const SourceFileRef& getSourceFile() const	{ return mSourceFile; }

	//! Returns the frame of the last buffer underrun or 0 if none since the last time this method was called.
	uint64_t getLastUnderrun();
	//! Returns the frame of the last buffer overrun or 0 if none since the last time this method was called.
	uint64_t getLastOverrun();

protected:
	void readAsyncImpl();
	void readImpl();
	void seekImpl( size_t readPos );
	void destroyReadThreadImpl();

	std::vector<dsp::RingBuffer>				mRingBuffers;	// used to transfer samples from io to audio thread, one ring buffer per channel
	BufferDynamic								mIoBuffer;		// used to read samples from the file on read thread, resizeable so the ringbuffer can be filled

	SourceFileRef								mSourceFile;
	size_t										mBufferFramesThreshold, mRingBufferPaddingFactor;
	std::atomic<uint64_t>						mLastUnderrun, mLastOverrun;

	std::unique_ptr<std::thread>				mReadThread;
	std::mutex									mAsyncReadMutex;
	std::condition_variable						mIssueAsyncReadCond;
	bool										mIsReadAsync, mAsyncReadShouldQuit;
};

} } // namespace cinder::audio2