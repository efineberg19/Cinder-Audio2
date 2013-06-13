#include "audio2/GeneratorNode.h"
#include "audio2/RingBuffer.h"
#include "cinder/Thread.h"
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

// TODO: make it possible to do the file read with just one buffer.
// - currently reading to mReadBuffer first, then copying that to mRingBuffer. This is required because
//   there is no way to move RingBuffer's write head forward without a write forward (write comes from
//   platform audio I/O)

struct FilePlayerNode::Impl {

	Impl() : mNumFramesBuffered( 0 ) {}

	void readFile( size_t readPosition );

	std::unique_ptr<std::thread> mReadThread;
	std::unique_ptr<RingBuffer> mRingBuffer;
	Buffer mReadBuffer;
	size_t mNumFramesBuffered;

	SourceFileRef mSourceFile;
};

FilePlayerNode::FilePlayerNode()
: PlayerNode()
{
}

FilePlayerNode::~FilePlayerNode()
{
}

FilePlayerNode::FilePlayerNode( SourceFileRef sourceFile )
: PlayerNode(), mSourceFile( sourceFile ), mImpl( new Impl )
{
	mNumFrames = mSourceFile->getNumFrames();
}

void FilePlayerNode::initialize()
{
	mSourceFile->setNumChannels( mFormat.getNumChannels() );
	mSourceFile->setSampleRate( mFormat.getSampleRate() );

	mImpl->mReadBuffer = Buffer( mFormat.getNumChannels(), mSourceFile->getNumFrames() );
	mImpl->mSourceFile = mSourceFile;
}

void FilePlayerNode::start()
{
	CI_ASSERT( mSourceFile );

	mReadPos = 0;
	mEnabled = true;

	LOG_V << "started" << endl;
}

void FilePlayerNode::stop()
{
	mEnabled = false;

	LOG_V << "stopped" << endl;
}

void FilePlayerNode::process( Buffer *buffer )
{
	size_t readPos = mReadPos;
	mImpl->readFile( readPos );

	size_t numFrames = buffer->getNumFrames();
	size_t numBuffered = mImpl->mNumFramesBuffered;

	// FIXME: this will happen at the end of the file
	if( numBuffered < numFrames )
		return;

	size_t readCount = std::min( mNumFrames - readPos, numFrames );


	for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ ) {
		size_t count = mImpl->mRingBuffer->read( buffer->getChannel( ch ), readCount );
		CI_ASSERT( count != readCount );
	}

	if( readCount < numFrames  )
		mEnabled = false;
}

void FilePlayerNode::Impl::readFile( size_t readPosition )
{
	size_t numRead = mSourceFile->read( &mReadBuffer, readPosition );
	for( size_t ch = 0; ch < mReadBuffer.getNumChannels(); ch++ )
		mRingBuffer->write( mReadBuffer.getChannel( ch ), numRead );

	mNumFramesBuffered += numRead;
}

} // namespace audio2