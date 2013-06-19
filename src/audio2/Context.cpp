
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

Node::Node()
: mInitialized( false ), mEnabled( false )
{
	mSources.resize( 1 );
}

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
	dest->setSource( shared_from_this(), bus );
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
	return ( mFormat.getNumChannels() == sourceFormat.getNumChannels() );
}

size_t Node::getSampleRate() const
{
	return getContext()->getSampleRate();
}

size_t Node::getNumFramesPerBlock() const
{
	return getContext()->getNumFramesPerBlock();
}

void Node::fillFormatParamsFromParent()
{
	NodeRef parent = getParent();
	CI_ASSERT( parent );

	while( parent ) {
		fillFormatParamsFromFormat( parent->getFormat() );
		if( mFormat.isComplete() )
			break;
		parent = parent->getParent();
	}
	
	CI_ASSERT( mFormat.isComplete() );
}

void Node::fillFormatParamsFromSource()
{
	CI_ASSERT( ! mSources.empty() && mSources[0] );

	auto firstSource = mSources[0];
	fillFormatParamsFromFormat( firstSource->getFormat() );

	CI_ASSERT( mFormat.isComplete() );
}

void Node::fillFormatParamsFromFormat( const Format &otherFormat )
{
	if( ! mFormat.getNumChannels() )
		mFormat.setNumChannels( otherFormat.getNumChannels() );
}

void Node::setEnabled( bool enabled )
{
	if( enabled )
		start();
	else
		stop();
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

TapNode::TapNode( size_t numBufferedFrames )
: Node(), mNumBufferedFrames( numBufferedFrames )
{
	mTag = "BufferTap";
	mFormat.setAutoEnabled();
}

TapNode::~TapNode()
{
}

// TODO: make it possible for tap size to be auto-configured to input size
// - methinks it requires all nodes to be able to keep a blocksize
void TapNode::initialize()
{
	mCopiedBuffer = Buffer( mFormat.getNumChannels(), mNumBufferedFrames, Buffer::Format::NonInterleaved );
	for( size_t ch = 0; ch < mFormat.getNumChannels(); ch++ )
		mRingBuffers.push_back( unique_ptr<RingBuffer>( new RingBuffer( mNumBufferedFrames ) ) );

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
	if( mEnabled )
		return;
	CI_ASSERT( mRoot );
	mEnabled = true;
	
	start( mRoot );
}

void Context::stop()
{
	if( ! mEnabled )
		return;
	mEnabled = false;

	stop( mRoot );
}

void Context::setEnabled( bool enabled )
{
	if( enabled )
		start();
	else
		stop();
}

RootNodeRef Context::getRoot()
{
	if( ! mRoot )
		mRoot = createOutput();
	return mRoot;
}

void Context::start( NodeRef node )
{
	if( ! node )
		return;
	for( auto& source : node->getSources() )
		start( source );

	if( node->getFormat().isAutoEnabled() )
		node->start();
}

void Context::stop( NodeRef node )
{
	if( ! node )
		return;
	for( auto& source : node->getSources() )
		stop( source );

	if( node->getFormat().isAutoEnabled() )
		node->stop();
}

} // namespace audio2