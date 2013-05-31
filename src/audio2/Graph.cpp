
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

void Node::connect( NodeRef source )
{
	mSources[0] = source;
	mSources[0]->setParent( shared_from_this() );
}

bool Node::supportsSourceFormat( const Node::Format &sourceFormat ) const
{
	return ( mFormat.getNumChannels() == sourceFormat.getNumChannels() && mFormat.getSampleRate() == sourceFormat.getSampleRate() );
}

const Node::Format& Node::getSourceFormat()
{
	CI_ASSERT( ! mSources.empty() );
	return mSources[0]->mFormat;
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

void MixerNode::connect( NodeRef source )
{
	mSources.push_back( source );
	source->setParent( shared_from_this() );
}

void MixerNode::connect( NodeRef source, size_t bus )
{
	if( bus > mSources.size() )
		throw AudioExc( string( "Mixer bus " ) + ci::toString( bus ) + " out of range (max: " + ci::toString( mSources.size() ) + ")" );
	if( mSources[bus] )
		throw AudioExc(  string( "Mixer bus " ) + ci::toString( bus ) + " is already in use. Replacing busses not yet supported." ); // ???: replace?

	mSources[bus] = source;
	source->setParent( shared_from_this() );
}

TapNode::TapNode( size_t bufferSize )
: Node(), mBufferSize( bufferSize )
{
	mTag = "BufferTap";
	mSources.resize( 1 );
}

TapNode::~TapNode()
{
}

// TODO: make it possible for tap size to be auto-configured to input size
// - methinks it requires all nodes to be able to keep a blocksize
void TapNode::initialize()
{
	size_t numChannels = mFormat.getNumChannels();
	mCopiedBuffer.resize( numChannels );
	for( size_t ch = 0; ch < mFormat.getNumChannels(); ch++ ) {
		mCopiedBuffer[ch].resize( mBufferSize );
		mRingBuffers.push_back( unique_ptr<RingBuffer>( new RingBuffer( mBufferSize ) ) );
	}

	mInitialized = true;
}

const BufferT& TapNode::getBuffer()
{
	for( size_t ch = 0; ch < mFormat.getNumChannels(); ch++ )
		mRingBuffers[ch]->read( &mCopiedBuffer[ch] );

	return mCopiedBuffer;
}

const ChannelT& TapNode::getChannel( size_t channel )
{
	CI_ASSERT( channel < mCopiedBuffer.size() );

	ChannelT& buf = mCopiedBuffer[channel];
	mRingBuffers[channel]->read( &buf );

	return buf;
}

void TapNode::render( BufferT *buffer )
{
	CI_ASSERT( mFormat.getNumChannels() == buffer->size() );

	for( size_t ch = 0; ch < mFormat.getNumChannels(); ch++ )
		mRingBuffers[ch]->write( (*buffer)[ch] );
}

Graph::~Graph()
{
	if( mInitialized )
		uninitialize();
}

void Graph::initialize()
{
	mInitialized = true;
}

void Graph::uninitialize()
{
	mInitialized = false;
}

void Graph::start()
{
	if( mRunning )
		return;
	CI_ASSERT( mRoot );
	mRunning = true;
	start( mRoot );
}

void Graph::stop()
{
	if( ! mRunning )
		return;
	mRunning = false;
	stop( mRoot );
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