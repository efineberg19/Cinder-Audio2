#include "audio2/Graph.h"
#include "audio2/RingBuffer.h"
#include "audio2/assert.h"

using namespace std;

namespace audio2 {

	Node::~Node()
	{

	}

	const Node::Format& Node::getSourceFormat()
	{
		CI_ASSERT( ! mSources.empty() );
		return mSources[0]->mFormat;
	}

	// TODO: consider default implementing connect() in Node
	//	- would throw if Producer since nothing can connect to it
	//	- override in Mixer to push_back
	void Consumer::connect( NodeRef source )
	{
		if( mSources.empty() )
			mSources.resize( 1 );
		mSources[0] = source;
		mSources[0]->setParent( shared_from_this() );
	}

	void Effect::connect( NodeRef source )
	{
		if( mSources.empty() )
			mSources.resize( 1 );
		mSources[0] = source;
		mSources[0]->setParent( shared_from_this() );
	}

	void Mixer::connect( NodeRef source )
	{
		mSources.push_back( source );
		mSources.back()->setParent( shared_from_this() );
	}

	void Mixer::connect( NodeRef source, size_t bus )
	{
		// TODO: throw exception if bus count not enough, else set in sources. Blow away old node
	}

	BufferTap::BufferTap( size_t bufferSize )
	: Node(), mBufferSize( bufferSize )
	{
		mTag = "BufferTap";
	}

	BufferTap::~BufferTap()
	{
	}

	void BufferTap::initialize()
	{
		size_t numChannels = mFormat.getNumChannels();
		mCopiedBuffer.resize( numChannels );
		for( size_t ch = 0; ch < mFormat.getNumChannels(); ch++ ) {
			mCopiedBuffer[ch].resize( mBufferSize );
			mRingBuffers.push_back( unique_ptr<RingBuffer>( new RingBuffer( mBufferSize ) ) );
		}
	}

	void BufferTap::connect( NodeRef source )
	{
		if( mSources.empty() )
			mSources.resize( 1 );
		mSources[0] = source;
		mSources[0]->setParent( shared_from_this() );
	}

	const BufferT& BufferTap::getBuffer()
	{
		for( size_t ch = 0; ch < mFormat.getNumChannels(); ch++ )
			mRingBuffers[ch]->read( &mCopiedBuffer[ch] );

		return mCopiedBuffer;
	}

	const ChannelT& BufferTap::getChannel( size_t channel )
	{
		CI_ASSERT( channel < mCopiedBuffer.size() );

		ChannelT& buf = mCopiedBuffer[channel];
		mRingBuffers[channel]->read( &buf );

		return buf;
	}

	void BufferTap::render( BufferT *buffer )
	{
		CI_ASSERT( mFormat.getNumChannels() == buffer->size() );

		for( size_t ch = 0; ch < mFormat.getNumChannels(); ch++ )
			mRingBuffers[ch]->write( (*buffer)[ch] );
	}

	Graph::~Graph()
	{
		uninitialize();
	}

	void Graph::initialize()
	{
	}

	void Graph::uninitialize()
	{
	}

	void Graph::start()
	{
		if( mRunning )
			return;
		CI_ASSERT( mOutput );
		mRunning = true;
		start( mOutput );
	}

	void Graph::stop()
	{
		if( ! mRunning )
			return;
		mRunning = false;
		stop( mOutput );
	}

	void Graph::start( NodeRef node )
	{
		for( auto& source : node->getSources() )
			start( source );

		node->start();
	}

	void Graph::stop( NodeRef node )
	{
		for( auto& source : node->getSources() )
			stop( source );

		node->stop();
	}

} // namespace audio2