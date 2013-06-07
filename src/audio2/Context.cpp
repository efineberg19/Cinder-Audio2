
#include "audio2/Context.h"
#include "audio2/GeneratorNode.h"
#include "audio2/audio.h"
#include "audio2/RingBuffer.h"
#include "audio2/assert.h"

#include "cinder/Cinder.h"
#include "cinder/Utilities.h"

#if defined( CINDER_COCOA )
#include "audio2/EngineAudioUnit.h"
#elif defined( CINDER_MSW )
#include "audio2/msw/ContextXAudio.h"
#endif

using namespace std;

namespace audio2 {

Node::~Node()
{

}

NodeRef Node::connect( NodeRef source )
{
	mSources[0] = source;
	mSources[0]->setParent( shared_from_this() );

	return source;
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

NodeRef MixerNode::connect( NodeRef source )
{
	mSources.push_back( source );
	source->setParent( shared_from_this() );

	return source;
}

NodeRef MixerNode::connect( NodeRef source, size_t bus )
{
	if( bus > mSources.size() )
		throw AudioExc( string( "Mixer bus " ) + ci::toString( bus ) + " out of range (max: " + ci::toString( mSources.size() ) + ")" );
	if( mSources[bus] )
		throw AudioExc(  string( "Mixer bus " ) + ci::toString( bus ) + " is already in use. Replacing busses not yet supported." ); // ???: replace?

	mSources[bus] = source;
	source->setParent( shared_from_this() );

	return source;
}

// TODO: bufferSize is ambigious, rename to numFrames
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
	mCopiedBuffer = Buffer( mFormat.getNumChannels(), mBufferSize, Buffer::Format::NonInterleaved );
	for( size_t ch = 0; ch < mFormat.getNumChannels(); ch++ ) {
		mRingBuffers.push_back( unique_ptr<RingBuffer>( new RingBuffer( mBufferSize ) ) );
	}

	mInitialized = true;
}

const Buffer& TapNode::getBuffer()
{
	for( size_t ch = 0; ch < mFormat.getNumChannels(); ch++ )
		mRingBuffers[ch]->read( mCopiedBuffer.getChannel( ch ), mCopiedBuffer.getNumFrames() );

	return mCopiedBuffer;
}

// FIXME: samples will go out of whack if only one channel is pulled. add a fillCopiedBuffer private method
const float *TapNode::getChannel( size_t channel )
{
	CI_ASSERT( channel < mCopiedBuffer.getNumChannels() );

	float *buf = mCopiedBuffer.getChannel( channel );
	mRingBuffers[channel]->read( buf, mCopiedBuffer.getNumFrames() );

	return buf;
}

void TapNode::render( Buffer *buffer )
{
	for( size_t ch = 0; ch < mFormat.getNumChannels(); ch++ )
		mRingBuffers[ch]->write( buffer->getChannel( ch ), buffer->getNumFrames() );
}

Context* Context::instance()
{
	static Context *sInstance = 0;
	if( ! sInstance ) {
#if defined( CINDER_COCOA )
		sInstance = new EngineAudioUnit();
#elif defined( CINDER_MSW )
		sInstance = new msw::ContextXAudio();
#else
		// TODO: add hook here to get user defined engine impl
#error "not implemented."
#endif
	}
	return sInstance;
}

Context::~Context()
{
	if( mInitialized )
		uninitialize();
}

void Context::initialize()
{
	mInitialized = true;
}

void Context::uninitialize()
{
	mInitialized = false;
}

void Context::start()
{
	if( mRunning )
		return;
	CI_ASSERT( mRoot );
	mRunning = true;
	start( mRoot );
}

void Context::stop()
{
	if( ! mRunning )
		return;
	mRunning = false;
	stop( mRoot );
}

void Context::start( NodeRef node )
{
	if( ! node )
		return;
	for( auto& source : node->getSources() )
		start( source );

	node->start();
}

void Context::stop( NodeRef node )
{
	if( ! node )
		return;
	for( auto& source : node->getSources() )
		stop( source );

	node->stop();
}

} // namespace audio2