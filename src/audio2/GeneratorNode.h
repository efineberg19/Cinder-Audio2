#pragma once

#include "audio2/Context.h"
#include "audio2/audio.h"
#include "audio2/Device.h"
#include "audio2/Dsp.h"

namespace audio2 {

class GeneratorNode : public Node {
public:
	GeneratorNode() : Node() {}
	virtual ~GeneratorNode() {}
};

class InputNode : public GeneratorNode {
public:
	InputNode( DeviceRef device ) : GeneratorNode() {}
	virtual ~InputNode() {}

	virtual DeviceRef getDevice() = 0;
};

class FileInputNode : public GeneratorNode {
public:
	FileInputNode() : GeneratorNode() {}
	virtual ~FileInputNode() {}
};

template <typename UGenT>
struct UGenNode : public GeneratorNode {
	UGenNode()	{
		mTag = "UGenNode";
		mFormat.setWantsDefaultFormatFromParent();
	}

	virtual void initialize() override {
		mGen.setSampleRate( mFormat.getSampleRate() );
	}

	virtual void render( Buffer *buffer ) override {
		size_t count = buffer->getNumFrames();
		mGen.render( buffer->getChannel( 0 ), count );
		for( size_t ch = 1; ch < buffer->getNumChannels(); ch++ )
			memcpy( buffer->getChannel( ch ), buffer->getChannel( 0 ), count * sizeof( float ) );
	}

	UGenT mGen;
};

} // namespace audio2