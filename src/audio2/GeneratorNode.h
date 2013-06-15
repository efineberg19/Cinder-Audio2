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
	GeneratorNode();
	virtual ~GeneratorNode() {}
};

class InputNode : public GeneratorNode {
public:
	InputNode( DeviceRef device ) : GeneratorNode() {
		mFormat.setAutoEnabled();
	}
	virtual ~InputNode() {}

	virtual DeviceRef getDevice() = 0;
};

//! \brief Abstract Node class for recorded audio playback
class PlayerNode : public GeneratorNode {
public:
	PlayerNode() : GeneratorNode() { mTag = "PlayerNode"; }
	virtual ~PlayerNode() {}

	void setReadPosition( size_t pos )	{ mReadPos = pos; }
	size_t getReadPosition() const	{ return mReadPos; }

	void setLoop( bool b = true )	{ mLoop = b; }
	bool getLoop() const			{ return mLoop; }

	size_t getNumFrames() const	{ return mNumFrames; }

protected:
	size_t mNumFrames;
	std::atomic<size_t> mReadPos;
	std::atomic<bool>	mLoop;
};

class BufferPlayerNode : public PlayerNode {
public:
	BufferPlayerNode() : PlayerNode() { mTag = "BufferPlayerNode"; }
	BufferPlayerNode( BufferRef buffer );
	virtual ~BufferPlayerNode() {}

	virtual void start() override;
	virtual void stop() override;
	virtual void process( Buffer *buffer );

	BufferRef getBuffer() const	{ return mBuffer; }

protected:
	BufferRef mBuffer;
};

// TODO NEXT: implement FilePlayerNode
//		- decodes and writes samples to ringbuffer on background thread
//		- pulls samples from ringbuffer in process()
//		- in a real-time graph, file reading needs to be done on a non-audio thread.
//        But in processing mode, the same thread should be used as the one that process is called from

// TODO: use a thread pool to keep the overrall number of read threads to a minimum.

class FilePlayerNode : public PlayerNode {
public:
	FilePlayerNode();
	FilePlayerNode( SourceFileRef sourceFile );
	virtual ~FilePlayerNode();

	void initialize() override;

	virtual void start() override;
	virtual void stop() override;
	virtual void process( Buffer *buffer );
  protected:

	void readFile( size_t numFramesPerBlock );

	std::unique_ptr<std::thread> mReadThread;
	std::unique_ptr<RingBuffer> mRingBuffer;
	Buffer mReadBuffer;
	size_t mNumFramesBuffered;

	SourceFileRef mSourceFile;
	size_t mBufferFramesThreshold;
};

template <typename UGenT>
struct UGenNode : public GeneratorNode {
	UGenNode() : GeneratorNode() {
		mTag = "UGenNode";
	}

	virtual void initialize() override {
		mGen.setSampleRate( mFormat.getSampleRate() );
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