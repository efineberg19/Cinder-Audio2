#include "audio2/GeneratorNode.h"
#include "audio2/RingBuffer.h"
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

void BufferPlayerNode::process( Buffer *buffer )
{
	size_t readPos = mReadPos;
	size_t numFrames = buffer->getNumFrames();
	size_t readCount = std::min( mNumFrames - readPos, numFrames );

	for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ )
		std::memcpy( buffer->getChannel( ch ), &mBuffer->getChannel( ch )[readPos], readCount * sizeof( float ) );

	if( readCount < numFrames  ) {
		buffer->zero( readCount, numFrames - readCount );

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


FilePlayerNode::FilePlayerNode()
: PlayerNode(), mNumFramesBuffered( 0 )
{
}

FilePlayerNode::~FilePlayerNode()
{
}

FilePlayerNode::FilePlayerNode( SourceFileRef sourceFile )
: PlayerNode(), mSourceFile( sourceFile ), mNumFramesBuffered( 0 )
{
	mTag = "FilePlayerNode";
	mNumFrames = mSourceFile->getNumFrames();
	mBufferFramesThreshold = 512; // TODO: expose
}

void FilePlayerNode::initialize()
{
	mSourceFile->setNumChannels( mFormat.getNumChannels() );
	mSourceFile->setSampleRate( mFormat.getSampleRate() );

	size_t paddingMultiplier = 2; // TODO: expose
	mReadBuffer = Buffer( mFormat.getNumChannels(), mSourceFile->getNumFramesPerRead() );
	mRingBuffer = unique_ptr<RingBuffer>( new RingBuffer( mFormat.getNumChannels() * mSourceFile->getNumFramesPerRead() * paddingMultiplier ) );
}

void FilePlayerNode::setReadPosition( size_t pos )
{
	CI_ASSERT( mSourceFile );

	mSourceFile->seek( pos );
	mReadPos = pos;
}

void FilePlayerNode::start()
{
	CI_ASSERT( mSourceFile );

	setReadPosition( 0 );
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

	size_t numFrames = buffer->getNumFrames();
	readFile( numFrames );

	size_t readCount = std::min( mNumFramesBuffered, numFrames );

	for( size_t ch = 0; ch < buffer->getNumChannels(); ch++ ) {
		size_t count = mRingBuffer->read( buffer->getChannel( ch ), readCount );
		if( count != readCount )
			LOG_V << "unexpected read count: " << count << ", expected: " << readCount << " (ch = " << ch << ")" << endl;
	}
	mNumFramesBuffered -= readCount;

	// check if end of file
	if( readCount < numFrames ) {
		buffer->zero( readCount, numFrames - readCount );

		if( mLoop ) {
			setReadPosition( 0 );
			return;
		} else
			mEnabled = false;
	}
}

// FIXME: this copy is really janky
// - mReadBuffer and mRingBuffer are much bigger than the block size
// - since they are non-interleaved, need to pack them in sections = numFramesPerBlock so stereo channels can be
//   pulled out appropriately.
// - Ideally, there would only be one buffer copied to on the background thread and then one copy/consume in process()	

void FilePlayerNode::readFile( size_t numFramesPerBlock )
{
	size_t readPos = mReadPos;

	if( mNumFramesBuffered >= mBufferFramesThreshold || readPos >= mNumFrames )
		return;

	size_t numRead = mSourceFile->read( &mReadBuffer );
	mReadPos += numRead;

	size_t channelOffset = 0;
	while( numRead > 0 ) {
		size_t writeCount = std::min( numFramesPerBlock, numRead );
		for( size_t ch = 0; ch < mReadBuffer.getNumChannels(); ch++ )
			mRingBuffer->write( mReadBuffer.getChannel( ch ) + channelOffset, writeCount );

		channelOffset += writeCount;

		numRead -= writeCount;
		CI_ASSERT( numRead < mReadBuffer.getSize() );
		mNumFramesBuffered += writeCount;
	}
}

} // namespace audio2