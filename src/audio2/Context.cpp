
#include "audio2/Context.h"
#include "audio2/GeneratorNode.h"
#include "audio2/audio.h"
#include "audio2/RingBuffer.h"
#include "audio2/assert.h"

#include "cinder/Cinder.h"
#include "cinder/Utilities.h"

#if defined( CINDER_COCOA )
#include "audio2/cocoa/ContextAudioUnit.h"
#elif defined( CINDER_MSW )
#include "audio2/msw/ContextXAudio.h"
#endif

using namespace std;

namespace audio2 {

Node::~Node()
{

}

NodeRef Node::connect( NodeRef dest )
{
	dest->setSource( shared_from_this() );
	return dest;
}

NodeRef Node::connect( NodeRef dest, size_t bus )
{
	dest->setSource( shared_from_this(), 0 );
	return dest;
}

void Node::setSource( NodeRef source )
{
	setSource( source, 0 );
}

// TODO: figure out how to best handle node replacements
void Node::setSource( NodeRef source, size_t bus )
{
	if( bus > mSources.size() )
		throw AudioExc( string( "bus " ) + ci::toString( bus ) + " is out of range (max: " + ci::toString( mSources.size() ) + ")" );
//	if( sources[bus] )
//		throw AudioExc(  string( "bus " ) + ci::toString( bus ) + " is already in use. Replacing busses not yet supported." );

	mSources[bus] = source;
	source->setParent( shared_from_this() );
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

void MixerNode::setSource( NodeRef source )
{
	source->setParent( shared_from_this() );

	for( size_t i = 0; i < mSources.size(); i++ ) {
		if( ! mSources[i] ) {
			mSources[i] = source;
			return;
		}
	}
	// all slots full, append
	mSources.push_back( source );
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

void TapNode::process( Buffer *buffer )
{
	for( size_t ch = 0; ch < mFormat.getNumChannels(); ch++ )
		mRingBuffers[ch]->write( buffer->getChannel( ch ), buffer->getNumFrames() );
}

Context* Context::instance()
{
	static Context *sInstance = 0;
	if( ! sInstance ) {
#if defined( CINDER_COCOA )
		sInstance = new cocoa::ContextAudioUnit();
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
	
//	start( mRoot );
	mRoot->start();
}

void Context::stop()
{
	if( ! mRunning )
		return;
	mRunning = false;

//	stop( mRoot );
	mRoot->stop();
}

RootNodeRef Context::getRoot()
{
	if( ! mRoot )
		mRoot = createOutput();
	return mRoot;
}

void Context::setRunning( bool running )
{
	if( running )
		start();
	else
		stop();
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