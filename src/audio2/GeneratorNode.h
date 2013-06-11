#pragma once

#include "audio2/Context.h"
#include "audio2/audio.h"
#include "audio2/Device.h"
#include "audio2/Dsp.h"

#include "cinder/DataSource.h"

namespace audio2 {

// TODO: sort the naming of 'Source' out.
// - I'd like to use SourceNode instead of GeneratorNode, but I'm already using getSources() and setSource()
//	 to mean upstream (children) nodes
// - would like to use Input / Output for that, but those terms are already used for device input / output

typedef std::shared_ptr<class BufferInputNode> BufferInputNodeRef;

class GeneratorNode : public Node {
public:
	GeneratorNode() : Node() {
		mFormat.setWantsDefaultFormatFromParent();
	}
	virtual ~GeneratorNode() {}
};

class InputNode : public GeneratorNode {
public:
	InputNode( DeviceRef device ) : GeneratorNode() {}
	virtual ~InputNode() {}

	virtual DeviceRef getDevice() = 0;
};

class BufferInputNode : public GeneratorNode {
public:
	BufferInputNode() : GeneratorNode() {}
	BufferInputNode( BufferRef inputBuffer );
	virtual ~BufferInputNode() {}

	virtual void process( Buffer *buffer );
private:
	BufferRef mBuffer;
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