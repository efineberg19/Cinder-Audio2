#pragma once

#include "audio2/Context.h"

namespace audio2 {

class RingBuffer;

class TapNode : public Node {
public:
	TapNode( size_t numBufferedFrames = 1024 );
	virtual ~TapNode();

	const float* getChannel( size_t ch = 0 );
	const Buffer& getBuffer();

	virtual void initialize() override;
	virtual void process( Buffer *buffer ) override;

private:
	std::vector<std::unique_ptr<RingBuffer> > mRingBuffers; // TODO: make this one continuous buffer so it better matches audio::Buffer
	Buffer mCopiedBuffer;
	size_t mNumBufferedFrames;
};


} // namespace audio2