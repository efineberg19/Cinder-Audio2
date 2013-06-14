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

	void readFile( size_t readPosition, size_t numFramesPerBlock );

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
	mTag = "FilePlayerNode";
	mNumFrames = mSourceFile->getNumFrames();
	mBufferFramesThreshold = 1024; // TODO: expose
}

void FilePlayerNode::initialize()
{
	mSourceFile->setNumChannels( mFormat.getNumChannels() );
	mSourceFile->setSampleRate( mFormat.getSampleRate() );

	mImpl->mReadBuffer = Buffer( mFormat.getNumChannels(), mSourceFile->getNumFramesPerRead() );
	mImpl->mRingBuffer = unique_ptr<RingBuffer>( new RingBuffer( mFormat.getNumChannels() * mSourceFile->getNumFramesPerRead() * 2 ) );
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
	size_t numFrames = buffer->getNumFrames();
	size_t numFramesLeft = mNumFrames - readPos;
//	app::console() << "------------- readPos: " << readPos << " -----------------" << endl;
	if( mImpl->mNumFramesBuffered < mBufferFramesThreshold )
		mImpl->readFile( readPos + mImpl->mNumFramesBuffered, numFrames );

	size_t numBuffered = mImpl->mNumFramesBuffered;
	size_t readCount = std::min( mNumFrames - readPos, numFrames );

//	app::console() << "numBuffered: " << numBuffered << ", readCount: " << readCount << endl;

	// check if we just don't have enough samples buffer to play
	if( numBuffered < numFrames && readCount > numFrames )
		return;

	for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ ) {
		size_t count = mImpl->mRingBuffer->read( buffer->getChannel( ch ), readCount );
		if( count != numFrames )
			LOG_V << " Warning, unexpected read count: " << count << ", expected: " << numFrames << " (ch = " << ch << ")" << endl;
	}

	// check if end of file
	if( readCount < numFrames  )
		mEnabled = false;

	mReadPos += readCount;
	mImpl->mNumFramesBuffered -= readCount;
}

// FIXME: this copy is really janky
// - mReadBuffer and mRingBuffer are much bigger than the block size
// - since they are non-interleaved, need to pack them in sections = numFramesPerBlock so stereo channels can be
//   pulled out appropriately.
// - Ideally, there would only be one buffer copied to on the background thread and then one copy/consume in process()
void FilePlayerNode::Impl::readFile( size_t readPosition, size_t numFramesPerBlock )
{
	size_t numRead = mSourceFile->read( &mReadBuffer, readPosition );
	CI_ASSERT( numRead < mReadBuffer.getSize() );
	app::console() << "BUFFER (" << numRead << ")" << endl;
	if( numRead == 272 )
		int blarg = 0;
	while( numRead > 0 ) {
		size_t writeCount = std::min( numFramesPerBlock, numRead );
		for( size_t ch = 0; ch < mReadBuffer.getNumChannels(); ch++ )
			mRingBuffer->write( mReadBuffer.getChannel( ch ), numFramesPerBlock );

		numRead -= writeCount;
		CI_ASSERT( numRead < mReadBuffer.getSize() );
		mNumFramesBuffered += writeCount;
	}
}

} // namespace audio2