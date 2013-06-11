#include "audio2/GeneratorNode.h"

#include "audio2/Debug.h"

using namespace ci;
using namespace std;

namespace audio2 {

GeneratorNode::GeneratorNode() : Node()
{
	LOG_V << "bang" << std::endl;
	mFormat.setWantsDefaultFormatFromParent();
}

BufferInputNode::BufferInputNode( BufferRef inputBuffer )
: GeneratorNode(), mBuffer( inputBuffer )
{
	mTag = "BufferInputNode";
	mNumFrames = mBuffer->getNumFrames();
}

void BufferInputNode::start()
{
	CI_ASSERT( mBuffer );

	mReadPos = 0;
	mRunning = true;

	LOG_V << "started" << endl;
}

void BufferInputNode::stop()
{
	mRunning = false;

	LOG_V << "stopped" << endl;
}

// TODO: silence when we have nothing to play
void BufferInputNode::process( Buffer *buffer )
{
	if( ! mRunning )
		return;

	size_t readPos = mReadPos;
	size_t readCount = std::min( mNumFrames - readPos, buffer->getNumFrames() );

	if( ! readCount ) {
		LOG_V << "read finished, stopping." << endl;
		mRunning = false;
		return;
	}

	for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ )
		std::memcpy( buffer->getChannel( ch ), &mBuffer->getChannel( ch )[readPos], readCount * sizeof( float ) );

	mReadPos += readCount;
}

} // namespace audio2