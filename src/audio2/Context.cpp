/*
 Copyright (c) 2013, The Cinder Project

 This code is intended to be used with the Cinder C++ library, http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/


#include "audio2/Context.h"
#include "audio2/GeneratorNode.h"
#include "audio2/audio.h"
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

// ----------------------------------------------------------------------------------------------------
// MARK: - Node
// ----------------------------------------------------------------------------------------------------

Node::Node( const Format &format )
: mInitialized( false ), mEnabled( false ), mSources( 1 ), mWantsDefaultFormatFromParent( format.getWantsDefaultFormatFromParent() ),
	mNumChannels( format.getChannels() ), mBufferLayout( Buffer::Layout::NonInterleaved ), mAutoEnabled( false )
{
	mNumChannelsUnspecified = ! format.getChannels();
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

void Node::disconnect( size_t bus )
{
	if( mEnabled )
		stop();
	
	auto& sources = getParent()->getSources();
	if( bus < sources.size() )
		sources[bus].reset();

	mParent.reset();
}

void Node::setSource( NodeRef source )
{
	setSource( source, 0 );
}

// TODO: figure out how to best handle node replacements
void Node::setSource( NodeRef source, size_t bus )
{
	CI_ASSERT( source != shared_from_this() );
	
	if( bus > mSources.size() )
		throw AudioExc( string( "bus " ) + ci::toString( bus ) + " is out of range (max: " + ci::toString( mSources.size() ) + ")" );
//	if( sources[bus] )
//		throw AudioExc(  string( "bus " ) + ci::toString( bus ) + " is already in use. Replacing busses not yet supported." );

	mSources[bus] = source;
	source->setParent( shared_from_this() );
}


void Node::fillFormatParamsFromParent()
{
	NodeRef parent = getParent();
	CI_ASSERT( parent );

	while( parent && ! mNumChannels ) {
		fillFormatParamsFromNode( parent );
		parent = parent->getParent();
	}
	
	CI_ASSERT( mNumChannels );
}

void Node::fillFormatParamsFromSource()
{
	CI_ASSERT( ! mSources.empty() && mSources[0] );

	auto firstSource = mSources[0];
	fillFormatParamsFromNode( firstSource );

	CI_ASSERT( mNumChannels );
}

void Node::fillFormatParamsFromNode( const NodeRef &otherNode )
{
	mNumChannels = otherNode->getNumChannels();
}

void Node::setEnabled( bool enabled )
{
	if( enabled )
		start();
	else
		stop();
}

// ----------------------------------------------------------------------------------------------------
// MARK: - MixerNode
// ----------------------------------------------------------------------------------------------------

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


// ----------------------------------------------------------------------------------------------------
// MARK: - Context
// ----------------------------------------------------------------------------------------------------

Context* Context::instance()
{
	static Context *sInstance = 0;
	if( ! sInstance ) {
#if defined( CINDER_COCOA )
		sInstance = new cocoa::ContextAudioUnit();
#elif defined( CINDER_MSW )
		sInstance = new msw::ContextXAudio();
#else
		throw AudioContextExc( "no default context for this platform." );
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
		mRoot = createLineOut();
	return mRoot;
}

void Context::start( NodeRef node )
{
	if( ! node )
		return;
	for( auto& source : node->getSources() )
		start( source );

	if( node->isAutoEnabled() )
		node->start();
}

void Context::stop( NodeRef node )
{
	if( ! node )
		return;
	for( auto& source : node->getSources() )
		stop( source );

	if( node->isAutoEnabled() )
		node->stop();
}

} // namespace audio2