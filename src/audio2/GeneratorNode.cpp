#include "audio2/GeneratorNode.h"

using namespace ci;

namespace audio2 {

BufferInputNode::BufferInputNode( SourceBufferRef sourceBuffer )
: mSourceBuffer( sourceBuffer )
{
	mTag = "BufferInputNode";
}

void BufferInputNode::process( Buffer *buffer )
{

}

} // namespace audio2