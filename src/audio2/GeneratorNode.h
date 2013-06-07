#pragma once

#include "audio2/Context.h"
#include "audio2/audio.h"
#include "audio2/Device.h"
#include "audio2/Dsp.h"

namespace audio2 {

//typedef std::shared_ptr<class GeneratorNode> GeneratorNodeRef;
//typedef std::shared_ptr<class InputNode> InputNodeRef;
//typedef std::shared_ptr<class FileInputNode> FileInputNodeRef;

class GeneratorNode : public Node {
public:
	GeneratorNode() : Node() {}
	virtual ~GeneratorNode() {}

	// TODO: consider making this private.
	// - it can still be called by typecasting to Node first, and that may also be more confusing than throwing
	NodeRef connect( NodeRef source ) override	{ throw AudioContextExc( "cannot connect a source to Node of type Generator" ); }
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