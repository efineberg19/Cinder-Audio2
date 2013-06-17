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

private:
	// note: GeneratorNode's cannot have any sources
	void setSource( NodeRef source ) override {}
	void setSource( NodeRef source, size_t bus ) override {}
};

class InputNode : public GeneratorNode {
public:
	InputNode( DeviceRef device ) : GeneratorNode() {
		mFormat.setAutoEnabled();
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
	PlayerNode() : GeneratorNode(), mNumFrames( 0 ), mReadPos( 0 ), mLoop( false ) { mTag = "PlayerNode"; }
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

// TODO: use a thread pool to keep the overrall number of read threads to a minimum.
class FilePlayerNode : public PlayerNode {
public:
	FilePlayerNode();
	FilePlayerNode( SourceFileRef sourceFile, bool isMultiThreaded = true );
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
	bool mMultiThreaded;
	std::atomic<bool> mReadOnBackground;

	std::atomic<size_t> mFramesPerBlock; // TODO: this is a kludge, remove once Node::Format has a blocksize
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