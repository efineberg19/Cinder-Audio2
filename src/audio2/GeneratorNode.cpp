#include "audio2/GeneratorNode.h"
#include "audio2/RingBuffer.h"
#include "audio2/Debug.h"

#include "cinder/Utilities.h"

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

FilePlayerNode::FilePlayerNode( SourceFileRef sourceFile, bool isMultiThreaded )
: PlayerNode(), mSourceFile( sourceFile ), mMultiThreaded( isMultiThreaded ), mNumFramesBuffered( 0 )
{
	mTag = "FilePlayerNode";
	mNumFrames = mSourceFile->getNumFrames();
	mBufferFramesThreshold = mSourceFile->getNumFramesPerRead() / 2; // TODO: expose

	mFramesPerBlock = 512; // kludge #2..
}

void FilePlayerNode::initialize()
{
	mSourceFile->setNumChannels( mFormat.getNumChannels() );
	mSourceFile->setSampleRate( mFormat.getSampleRate() );

	size_t paddingMultiplier = 2; // TODO: expose
	mReadBuffer = Buffer( mFormat.getNumChannels(), mSourceFile->getNumFramesPerRead() );
	mRingBuffer = unique_ptr<RingBuffer>( new RingBuffer( mFormat.getNumChannels() * mSourceFile->getNumFramesPerRead() * paddingMultiplier ) );

	if( mMultiThreaded ) {
		mReadOnBackground = true;
		mReadThread = unique_ptr<thread>( new thread( bind( &FilePlayerNode::readFromBackgroundThread, this ) ) );
	}
}

void FilePlayerNode::uninitialize()
{
	if( mMultiThreaded ) {
		mReadOnBackground = false;
		mReadThread->detach();
	}
}

void FilePlayerNode::setReadPosition( size_t pos )
{
	CI_ASSERT( mSourceFile );

	if( ! mMultiThreaded )
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

	if( ! mMultiThreaded && moreFramesNeeded() )
		readFile();

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

		if( mReadPos >= mNumFrames ) {
			if( mLoop ) {
				setReadPosition( 0 );
				return;
			}

			mEnabled = false;
		}
		else
			LOG_V << "BUFFER UNDERRUN" << endl;
	}
}

bool FilePlayerNode::moreFramesNeeded()
{
	return ( mNumFramesBuffered < mBufferFramesThreshold && mReadPos < mNumFrames ) ? true : false;
}

void FilePlayerNode::readFromBackgroundThread()
{
	size_t readMilliseconds = ( 1000 * mSourceFile->getNumFramesPerRead() ) / mFormat.getSampleRate();
	size_t lastReadPos = mReadPos;
	while( mReadOnBackground ) {
		if( ! moreFramesNeeded() ) {
			// FIXME: this is still causing underruns. need either:
			// a) a higher resolution timer
			// b) condition + mutex
			ci::sleep( readMilliseconds / 2 );
			continue;
		}

		size_t readPos = mReadPos;
		if( readPos != lastReadPos )
			mSourceFile->seek( readPos );

		readFile();
	}
}

// FIXME: this copy is really janky
// - mReadBuffer and mRingBuffer are much bigger than the block size
// - since they are non-interleaved, need to pack them in sections = numFramesPerBlock so stereo channels can be
//   pulled out appropriately.
// - Ideally, there would only be one buffer copied to on the background thread and then one copy/consume in process()	

void FilePlayerNode::readFile()
{
	size_t numRead = mSourceFile->read( &mReadBuffer );
	mReadPos += numRead;

	size_t numFramesPerBlock = mFramesPerBlock;
	size_t channelOffset = 0;
	while( numRead ) {
		CI_ASSERT( numRead <= mNumFrames );

		size_t writeCount = std::min( numFramesPerBlock, numRead );
		for( size_t ch = 0; ch < mReadBuffer.getNumChannels(); ch++ )
			mRingBuffer->write( mReadBuffer.getChannel( ch ) + channelOffset, writeCount );

		channelOffset += writeCount;

		numRead -= writeCount;
		mNumFramesBuffered += writeCount;
	}
}

} // namespace audio2