#pragma once

#include "audio2/Context.h"
#include "audio2/audio.h"
#include "audio2/Device.h"
#include "audio2/Dsp.h"
#include "audio2/Atomic.h"

#include "cinder/DataSource.h"

//#include "audio2/Debug.h"

namespace audio2 {

// TODO: sort the naming of 'Source' out.
// - I'd like to use SourceNode instead of GeneratorNode, but I'm already using getSources() and setSource()
//	 to mean upstream (children) nodes
// - would like to use Input / Output for that, but those terms are already used for device input / output

// - possible: DeviceInputNode : public InputNode, DeviceOutputNode : public OutputNode
//		- this is still confusing if you have Node::mOutput / Node::mInputs
// - webaudio uses LiveInput - use that and LiveOutput? I think I prefer MicInput / SpeakerOutput, even though that isn't always the case
// - pd uses DAC / ADC, which is confusing for some

typedef std::shared_ptr<class BufferInputNode> BufferInputNodeRef;

class GeneratorNode : public Node {
public:
	GeneratorNode();

	// TODO: think I found another compiler bug...
	// GeneratorNode() is not called from BufferInputNode's constructor unless it is in the cpp.
	//	- but then maybe this is one of those libc++ hokey rules. check on vc11 to verify
//	GeneratorNode() : Node() {
//		LOG_V << "SHHHAAAAZZAAMMM!!!" << std::endl;
//		mFormat.setWantsDefaultFormatFromParent();
//	}
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

// TODO: rename BufferPlayerNode / FilePlayerNode ?
class BufferInputNode : public GeneratorNode {
public:
	BufferInputNode() : GeneratorNode() {}
	BufferInputNode( BufferRef inputBuffer );
	virtual ~BufferInputNode() {}

	virtual void start() override;
	virtual void stop() override;
	virtual void process( Buffer *buffer );

	void setReadPosition( size_t pos )	{ mReadPos = pos; }
	size_t getReadPosition() const	{ return mReadPos; }

	BufferRef getBuffer() const	{ return mBuffer; }

	void setLoop( bool b = true )	{ mLoop = b; }
	bool getLoop() const			{ return mLoop; }
	
private:
	BufferRef mBuffer;
	size_t mNumFrames;
	std::atomic<size_t> mReadPos;
	std::atomic<bool>	mLoop;
};


class FileInputNode : public GeneratorNode {
public:
	FileInputNode() : GeneratorNode() {}
	virtual ~FileInputNode() {}
};

template <typename UGenT>
struct UGenNode : public GeneratorNode {
	UGenNode() : GeneratorNode()	{
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

	UGenT mGen;
};

} // namespace audio2