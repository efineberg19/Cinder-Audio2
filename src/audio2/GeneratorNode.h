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

#include "audio2/audio.h"
#include "audio2/File.h"
#include "audio2/Dsp.h"
#include "audio2/Atomic.h"

#include "cinder/DataSource.h"
#include "cinder/Thread.h"

namespace audio2 {

// TODO: sort the naming of 'Source' out.
// - I'd like to use SourceNode instead of GeneratorNode, but I'm already using getSources() and setSource()
//	 to mean upstream (children) nodes
// - would like to use Input / Output for that, but those terms are already used for device input / output

// - possible: DeviceInputNode : public InputNode, DeviceOutputNode : public OutputNode
//		- this is still confusing if you have Node::mOutput / Node::mInputs
// - webaudio uses LiveInput - use that and LiveOutput? I think I prefer MicInput / SpeakerOutput, even though that isn't always the case
// - pd uses DAC / ADC, which is confusing for some

typedef std::shared_ptr<class PlayerNode>		PlayerNodeRef;
typedef std::shared_ptr<class BufferPlayerNode> BufferPlayerNodeRef;
typedef std::shared_ptr<class FilePlayerNode>	FilePlayerNodeRef;

class RingBuffer;

class GeneratorNode : public Node {
public:
	GeneratorNode( const Format &format );
	virtual ~GeneratorNode() {}

private:
	// note: GeneratorNode's cannot have any sources
	void setSource( NodeRef source ) override {}
	void setSource( NodeRef source, size_t bus ) override {}
};

class InputNode : public GeneratorNode {
public:
	InputNode( DeviceRef device, const Format &format ) : GeneratorNode( format ) {
		setAutoEnabled();
	}
	virtual ~InputNode() {}

	virtual DeviceRef getDevice() = 0;
};

//! \brief Base Node class for recorded audio playback
//! \note PlayerNode itself doesn't process any audio.
//! \see BufferPlayerNode
//! \see FilePlayerNode
class PlayerNode : public GeneratorNode {
public:
	PlayerNode( const Format &format = Format() ) : GeneratorNode( format ), mNumFrames( 0 ), mReadPos( 0 ), mLoop( false ) { mTag = "PlayerNode"; }
	virtual ~PlayerNode() {}

	virtual void setReadPosition( size_t pos )	{ mReadPos = pos; }
	virtual size_t getReadPosition() const	{ return mReadPos; }

	virtual void setLoop( bool b = true )	{ mLoop = b; }
	virtual bool getLoop() const			{ return mLoop; }

	virtual size_t getNumFrames() const	{ return mNumFrames; }

protected:
	size_t mNumFrames;
	std::atomic<size_t> mReadPos;
	std::atomic<bool>	mLoop;
};

class BufferPlayerNode : public PlayerNode {
public:
	BufferPlayerNode( const Format &format = Format() ) : PlayerNode( format ) { mTag = "BufferPlayerNode"; }
	BufferPlayerNode( BufferRef buffer, const Format &format = Format() );
	virtual ~BufferPlayerNode() {}

	virtual void start() override;
	virtual void stop() override;
	virtual void process( Buffer *buffer );

	BufferRef getBuffer() const	{ return mBuffer; }

protected:
	BufferRef mBuffer;
};

// TODO: use a thread pool to keep the overrall number of read threads to a minimum.
class FilePlayerNode : public PlayerNode {
public:
	FilePlayerNode( const Format &format = Format() );
	FilePlayerNode( SourceFileRef sourceFile, bool isMultiThreaded = true, const Format &format = Node::Format() );
	virtual ~FilePlayerNode();

	void initialize() override;
	void uninitialize() override;

	virtual void start() override;
	virtual void stop() override;
	virtual void process( Buffer *buffer ) override;

	virtual void setReadPosition( size_t pos ) override;

	bool isMultiThreaded() const	{ return mMultiThreaded; }

  protected:

	void readFromBackgroundThread();
	void readFile();
	bool moreFramesNeeded();

	std::unique_ptr<std::thread> mReadThread;
	std::unique_ptr<RingBuffer> mRingBuffer;
	Buffer mReadBuffer;
	size_t mNumFramesBuffered;

	SourceFileRef mSourceFile;
	size_t mBufferFramesThreshold;
	size_t mSampleRate;
	bool mMultiThreaded;
	std::atomic<bool> mReadOnBackground;

	std::atomic<size_t> mFramesPerBlock;
};

template <typename UGenT>
struct UGenNode : public GeneratorNode {
	UGenNode( const Format &format = Format() ) : GeneratorNode( format ) {
		mTag = "UGenNode";
	}

	virtual void initialize() override {
		mGen.setSampleRate( getContext()->getSampleRate() );
	}

	virtual void process( Buffer *buffer ) override {
		size_t count = buffer->getNumFrames();
		mGen.process( buffer->getChannel( 0 ), count );
		for( size_t ch = 1; ch < buffer->getNumChannels(); ch++ )
			memcpy( buffer->getChannel( ch ), buffer->getChannel( 0 ), count * sizeof( float ) );
	}

	UGenT& getUGen()				{ return mGen; }
	const UGenT& getUGen() const	{ return mGen; }

protected:
	UGenT mGen;
};

} // namespace audio2