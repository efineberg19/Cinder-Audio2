#pragma once

#include "audio2/Graph.h"
#include "audio2/Device.h"
#include "audio2/Dsp.h"

namespace audio2 {

typedef std::shared_ptr<class Generator> GeneratorRef;

class Generator : public Node {
public:
	Generator() : Node() {}
	virtual ~Generator() {}
};

class Input : public Generator {
public:
	Input( DeviceRef device ) : Generator() {}
	virtual ~Input() {}

	virtual DeviceRef getDevice() = 0;
};

template <typename UGenT>
struct UGenNode : public Generator {
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