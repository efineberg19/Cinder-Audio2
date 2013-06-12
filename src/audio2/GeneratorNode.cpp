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
	mFormat.setNumChannels( mBuffer->getNumChannels() );
}

void BufferInputNode::start()
{
	CI_ASSERT( mBuffer );

	mReadPos = 0;
	mEnabled = true;

	LOG_V << "started" << endl;
}

void BufferInputNode::stop()
{
	mEnabled = false;

	LOG_V << "stopped" << endl;
}

// TODO: consider moving the copy to a Buffer method?
void BufferInputNode::process( Buffer *buffer )
{
	if( ! mEnabled )
		return;

	size_t readPos = mReadPos;
	size_t numFrames = buffer->getNumFrames();
	size_t readCount = std::min( mNumFrames - readPos, numFrames );

	for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ )
		std::memcpy( buffer->getChannel( ch ), &mBuffer->getChannel( ch )[readPos], readCount * sizeof( float ) );

	if( readCount < numFrames  ) {
		size_t numLeft = numFrames - readCount;
		for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ )
			std::memset( &buffer->getChannel( ch )[readCount], 0, numLeft * sizeof( float ) );

		// TODO: check for loop and restart if yes
		mEnabled = false;
	}

	mReadPos += readCount;
}

} // namespace audio2