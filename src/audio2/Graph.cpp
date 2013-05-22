
#include "cinder/Cinder.h"
#if defined( CINDER_MSW )
	// boost.lockfree performs some safe but unchecked calls to std::copy, that produces a nasty warning and this disabled it.
	// Unfortunately, it must be set from an implementation file.
	#pragma warning(disable:4996)
#endif

#include "audio2/Graph.h"
#include "audio2/audio.h"
#include "audio2/RingBuffer.h"
#include "audio2/assert.h"

#include "cinder/Utilities.h"

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
	source->setParent( shared_from_this() );
}

// TODO: Mixer connections need to be thought about more. Notes from discussion with Andrew:
// - connect( node ):
//		- will add node to next available unnused slot
//		- it always succeeds - increase mMaxNumBusses if necessary
//
// - connect( node, bus ):
//		- will throw if bus >= mMaxNumBusses
//		- if bus >= mSources.size(), resize mSources
//		- replaces any existing node at that spot
//			- what happens to it's connections?
// - Because of this functionality, the naming seems off:
//		- mMaxNumBusses should be mNumBusses, there is no max
//			- so there is getNumBusses() / setNumBusses()
//		- there can be 'holes', slots in mSources that are not used
//		- getNumActiveBusses() returns number of used slots

void Mixer::connect( NodeRef source )
{
	mSources.push_back( source );
	source->setParent( shared_from_this() );
}

void Mixer::connect( NodeRef source, size_t bus )
{
	if( bus > mSources.size() )
		throw AudioExc( string( "Mixer bus " ) + ci::toString( bus ) + " out of range (max: " + ci::toString( mSources.size() ) + ")" );
	if( mSources[bus] )
		throw AudioExc(  string( "Mixer bus " ) + ci::toString( bus ) + " is already in use. Replacing busses not yet supported." ); // ???: replace?

	mSources[bus] = source;
	source->setParent( shared_from_this() );
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
	if( ! node )
		return;
	for( auto& source : node->getSources() )
		start( source );

	node->start();
}

void Graph::stop( NodeRef node )
{
	if( ! node )
		return;
	for( auto& source : node->getSources() )
		stop( source );

	node->stop();
}

} // namespace audio2