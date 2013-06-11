#include "audio2/GeneratorNode.h"

using namespace ci;

namespace audio2 {

BufferInputNode::BufferInputNode( BufferRef inputBuffer )
: mBuffer( inputBuffer )
{
	mTag = "BufferInputNode";
}

void BufferInputNode::process( Buffer *buffer )
{
	// TODO NEXT
}

} // namespace audio2