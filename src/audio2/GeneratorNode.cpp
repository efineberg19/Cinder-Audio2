#include "audio2/GeneratorNode.h"

#include "audio2/Debug.h"

using namespace ci;
using namespace std;

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - GeneratorNode
// ----------------------------------------------------------------------------------------------------

GeneratorNode::GeneratorNode() : Node()
{
	mSources.clear();
	mFormat.setWantsDefaultFormatFromParent();
}

// ----------------------------------------------------------------------------------------------------
// MARK: - BufferPlayerNode
// ----------------------------------------------------------------------------------------------------

BufferPlayerNode::BufferPlayerNode( BufferRef buffer )
: PlayerNode(), mBuffer( buffer )
{
	mTag = "BufferPlayerNode";
	mNumFrames = mBuffer->getNumFrames();
	mFormat.setNumChannels( mBuffer->getNumChannels() );
}

void BufferPlayerNode::start()
{
	CI_ASSERT( mBuffer );

	mReadPos = 0;
	mEnabled = true;

	LOG_V << "started" << endl;
}

void BufferPlayerNode::stop()
{
	mEnabled = false;

	LOG_V << "stopped" << endl;
}

// TODO: consider moving the copy to a Buffer method
void BufferPlayerNode::process( Buffer *buffer )
{
	size_t readPos = mReadPos;
	size_t numFrames = buffer->getNumFrames();
	size_t readCount = std::min( mNumFrames - readPos, numFrames );

	for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ )
		std::memcpy( buffer->getChannel( ch ), &mBuffer->getChannel( ch )[readPos], readCount * sizeof( float ) );

	if( readCount < numFrames  ) {
		size_t numLeft = numFrames - readCount;
		for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ )
			std::memset( &buffer->getChannel( ch )[readCount], 0, numLeft * sizeof( float ) );

		if( mLoop ) {
			mReadPos = 0;
			return;
		} else
			mEnabled = false;
	}

	mReadPos += readCount;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - FilePlayerNode
// ----------------------------------------------------------------------------------------------------

FilePlayerNode::FilePlayerNode( SourceFileRef sourceFile )
: PlayerNode(), mSourceFile( sourceFile )
{
	mNumFrames = mSourceFile->getNumFrames();
	mFormat.setNumChannels( mSourceFile->getNumChannels() );
}

void FilePlayerNode::initialize()
{

}

void FilePlayerNode::start()
{

}

void FilePlayerNode::stop()
{

}

void FilePlayerNode::process( Buffer *buffer )
{

}

} // namespace audio2