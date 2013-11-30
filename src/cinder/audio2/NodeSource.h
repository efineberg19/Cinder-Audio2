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

#pragma once

#include "cinder/audio2/Node.h"
#include "cinder/audio2/Param.h"
#include "cinder/audio2/File.h"
#include "cinder/audio2/dsp/Dsp.h"
#include "cinder/audio2/dsp/RingBuffer.h"

#include "cinder/DataSource.h"

#include <thread>

namespace cinder { namespace audio2 {

typedef std::shared_ptr<class NodeSource>					NodeSourceRef;
typedef std::shared_ptr<class LineIn>					LineInRef;
typedef std::shared_ptr<class SamplePlayer>				SamplePlayerRef;
typedef std::shared_ptr<class BufferPlayer>				BufferPlayerRef;
typedef std::shared_ptr<class FilePlayer>				FilePlayerRef;
typedef std::shared_ptr<class Gen>						GenRef;
typedef std::shared_ptr<class CallbackProcessor>		CallbackProcessorRef;

typedef std::function<void( Buffer *, size_t )> CallbackProcessorFn;

class NodeSource : public Node {
  public:
	std::string virtual getTag() override			{ return "NodeSource"; }
	virtual ~NodeSource();

  protected:
	NodeSource( const Format &format );
  private:
	// NodeSource's cannot have any sources, overridden to assert this method isn't called
	void connectInput( const NodeRef &input, size_t bus ) override;
};

class LineIn : public NodeSource {
public:
	virtual ~LineIn();

	std::string virtual getTag() override			{ return "LineIn"; }

	//! Returns the associated \a Device.
	virtual const DeviceRef& getDevice() const		{ return mDevice; }
	//! Returns the frame of the last buffer underrun or 0 if none since the last time this method was called.
	virtual uint64_t getLastUnderrun() = 0;
	//! Returns the frame of the last buffer overrun or 0 if none since the last time this method was called.
	virtual uint64_t getLastOverrun() = 0;

protected:
	LineIn( const DeviceRef &device, const Format &format );

	DeviceRef	mDevice;
};

//! \brief Base Node class for sampled audio playback
//! \note SamplePlayer itself doesn't process any audio, but contains the common interface for Node's that do.
//! \see BufferPlayer
//! \see FilePlayer
class SamplePlayer : public NodeSource {
public:
	std::string virtual getTag() override			{ return "SamplePlayer"; }

	//! Seek the read position to \a readPositionFrames
	virtual void seek( size_t readPositionFrames ) = 0;
	//! Seek to read position \a readPositionSeconds
	void seekToTime( double readPositionSeconds );
	//! Gets the current read position in frames
	size_t getReadPosition() const	{ return mReadPos; }
	//! Gets the current read position in seconds.
	double getReadPositionTime() const;

	virtual void setLoop( bool b = true )	{ mLoop = b; }
	virtual bool getLoop() const			{ return mLoop; }

	virtual size_t getNumFrames() const	{ return mNumFrames; }

protected:
	SamplePlayer( const Format &format = Format() ) : NodeSource( format ), mNumFrames( 0 ), mReadPos( 0 ), mLoop( false ) {}
	virtual ~SamplePlayer() {}

	size_t mNumFrames;
	std::atomic<size_t> mReadPos;
	std::atomic<bool>	mLoop;
};

//! Buffer-based sample player.
class BufferPlayer : public SamplePlayer {
public:
	//! Constructs a BufferPlayer without a buffer, with the assumption one will be set later. \note Format::channels() can still be used to allocate the expected channel count ahead of time.
	BufferPlayer( const Format &format = Format() );
	//! Constructs a BufferPlayer \a buffer. \note Channel mode is always ChannelMode::SPECIFIED and num channels matches \a buffer. Format::channels() is ignored.
	BufferPlayer( const BufferRef &buffer, const Format &format = Format() );
	virtual ~BufferPlayer() {}

	std::string virtual getTag() override			{ return "BufferPlayer"; }

	virtual void start() override;
	virtual void stop() override;
	virtual void seek( size_t readPositionFrames ) override;
	virtual void process( Buffer *buffer ) override;

	void setBuffer( const BufferRef &buffer );
	const BufferRef& getBuffer() const	{ return mBuffer; }

protected:
	BufferRef mBuffer;
};

class FilePlayer : public SamplePlayer {
public:
	FilePlayer( const Format &format = Format() );
	FilePlayer( const SourceFileRef &sourceFile, bool isMultiThreaded = true, const Format &format = Node::Format() );
	virtual ~FilePlayer();

	std::string virtual getTag() override			{ return "FilePlayer"; }

	void initialize() override;
	void uninitialize() override;

	virtual void start() override;
	virtual void stop() override;
	virtual void seek( size_t readPositionFrames ) override;
	virtual void process( Buffer *buffer ) override;

	bool isMultiThreaded() const	{ return mMultiThreaded; }

	void setSourceFile( const SourceFileRef &sourceFile );
	const SourceFileRef& getSourceFile() const	{ return mSourceFile; }

	//! Returns the frame of the last buffer underrun or 0 if none since the last time this method was called.
	uint64_t getLastUnderrun();
	//! Returns the frame of the last buffer overrun or 0 if none since the last time this method was called.
	uint64_t getLastOverrun();

  protected:

	void readFromBackgroundThread();
	void readFile();
	void destroyIoThread();

	std::unique_ptr<std::thread>				mReadThread;
	std::vector<dsp::RingBuffer>				mRingBuffers;	// used to transfer samples from io to audio thread, one ring buffer per channel
	BufferDynamic								mIoBuffer;		// used to read samples from the file on io thread, resizeable so the ringbuffer can be filled

	SourceFileRef								mSourceFile;
	size_t										mBufferFramesThreshold, mRingBufferPaddingFactor;
	bool										mMultiThreaded, mReadOnBackground;
	
	std::atomic<uint64_t>						mLastUnderrun, mLastOverrun;

	std::mutex				mIoMutex;
	std::condition_variable	mNeedMoreSamplesCond;
};

class CallbackProcessor : public NodeSource {
  public:
	CallbackProcessor( const CallbackProcessorFn &callbackFn, const Format &format = Format() ) : NodeSource( format ), mCallbackFn( callbackFn ) {}
	virtual ~CallbackProcessor() {}

	std::string virtual getTag() override			{ return "CallbackProcessor"; }

	void process( Buffer *buffer ) override;

  private:
	CallbackProcessorFn mCallbackFn;
};

class Gen : public NodeSource {
  public:
	Gen( const Format &format = Format() );

	void initialize() override;

	std::string virtual getTag() override			{ return "Gen"; }

	void setFreq( float freq )		{ mFreq.setValue( freq ); }
	float getFreq() const			{ return mFreq.getValue(); }

	Param* getParamFreq()			{ return &mFreq; }

  protected:
	float mSampleRate;

	Param mFreq;
	float mPhase;
};

//! \note freq param is ignored
class GenNoise : public Gen {
  public:
	GenNoise( const Format &format = Format() ) : Gen( format ) {}

	void process( Buffer *buffer ) override;
};

class GenPhasor : public Gen {
public:
	GenPhasor( const Format &format = Format() ) : Gen( format )
	{}

	void process( Buffer *buffer ) override;
};

class GenSine : public Gen {
  public:
	GenSine( const Format &format = Format() ) : Gen( format )
	{}

	void process( Buffer *buffer ) override;
};

class GenTriangle : public Gen {
  public:
	GenTriangle( const Format &format = Format() ) : Gen( format ), mUpSlope( 1.0f ), mDownSlope( 1.0f )
	{}

	void setUpSlope( float up )			{ mUpSlope = up; }
	void setDownSlope( float down )		{ mDownSlope = down; }

	float getUpSlope() const		{ return mUpSlope; }
	float getDownSlope() const		{ return mDownSlope; }

	void process( Buffer *buffer ) override;

  private:
	std::atomic<float> mUpSlope, mDownSlope;
};

} } // namespace cinder::audio2