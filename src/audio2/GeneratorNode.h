#pragma once

#include "audio2/Graph.h"
#include "audio2/audio.h"
#include "audio2/Device.h"
#include "audio2/Dsp.h"

namespace audio2 {

typedef std::shared_ptr<class GeneratorNode> GeneratorNodeRef;

class GeneratorNode : public Node {
public:
	GeneratorNode() : Node() {}
	virtual ~GeneratorNode() {}

	// TODO: consider making this private.
	// - it can still be called by typecasting to Node first, and that may also be more confusing than throwing
	void connect( NodeRef source ) override	{ throw AudioGraphExc( "cannot connect a source to Node of type Generator" ); }
};

class Input : public GeneratorNode {
public:
	Input( DeviceRef device ) : GeneratorNode() {}
	virtual ~Input() {}

	virtual DeviceRef getDevice() = 0;
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

	virtual void render( BufferT *buffer ) override {
		mGen.render( buffer );
	}

	UGenT mGen;
};

} // namespace audio2