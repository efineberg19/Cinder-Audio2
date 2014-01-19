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


#include "cinder/audio2/Context.h"
#include "cinder/audio2/NodeInput.h"
#include "cinder/audio2/Debug.h"

#include "cinder/Cinder.h"

#include "cinder/app/App.h"		// for app::console()

#if defined( CINDER_COCOA )
	#include "cinder/audio2/cocoa/ContextAudioUnit.h"
	#if defined( CINDER_MAC )
		#include "cinder/audio2/cocoa/DeviceManagerCoreAudio.h"
	#else // CINDER_COCOA_TOUCH
		#include "cinder/audio2/cocoa/DeviceManagerAudioSession.h"
	#endif
#elif defined( CINDER_MSW )
	#include "cinder/audio2/msw/ContextXAudio.h"
	#include "cinder/audio2/msw/DeviceManagerWasapi.h"
#endif

using namespace std;

namespace cinder { namespace audio2 {

std::shared_ptr<Context>		Context::sHardwareContext;
std::unique_ptr<DeviceManager>	Context::sDeviceManager;

Context* Context::master()
{
	if( ! sHardwareContext ) {
#if defined( CINDER_COCOA )
		sHardwareContext.reset( new cocoa::ContextAudioUnit() );
#elif defined( CINDER_MSW )
		sHardwareContext.reset( new msw::ContextXAudio() );
#endif
	}
	return sHardwareContext.get();
}

DeviceManager* Context::deviceManager()
{
	if( ! sDeviceManager ) {
#if defined( CINDER_MAC )
		sDeviceManager.reset( new cocoa::DeviceManagerCoreAudio() );
#elif defined( CINDER_COCOA_TOUCH )
		sDeviceManager.reset( new cocoa::DeviceManagerAudioSession() );
#elif defined( CINDER_MSW )
		sDeviceManager.reset( new msw::DeviceManagerWasapi() );
#endif
	}
	return sDeviceManager.get();
}

Context::~Context()
{
	stop();
	lock_guard<mutex> lock( mMutex );
	uninitializeAllNodes();
}

void Context::start()
{
	if( mEnabled )
		return;

	mEnabled = true;
	getOutput()->start();
}

void Context::stop()
{
	if( ! mEnabled )
		return;

	mEnabled = false;
	getOutput()->stop();
}

void Context::disconnectAllNodes()
{
	if( mEnabled )
		stop();
	
	disconnectRecursive( mOutput );
}

void Context::setEnabled( bool enabled )
{
	if( enabled )
		start();
	else
		stop();
}

void Context::setOutput( const NodeOutputRef &output )
{
	mOutput = output;
}

const NodeOutputRef& Context::getOutput()
{
	if( ! mOutput )
		mOutput = createLineOut();
	return mOutput;
}

//void Context::startRecursive( const NodeRef &node )
//{
//	if( ! node )
//		return;
//	for( auto& input : node->getInputs() )
//		startRecursive( input );
//
//	if( node->isAutoEnabled() )
//		node->start();
//}

//void Context::stopRecursive( const NodeRef &node )
//{
//	if( ! node )
//		return;
//	for( auto& input : node->getInputs() )
//		stopRecursive( input );
//
//	if( node->isAutoEnabled() )
//		node->stop();
//}

void Context::disconnectRecursive( const NodeRef &node )
{
	if( ! node )
		return;
	for( auto &in : node->getInputs() )
		disconnectRecursive( in.second );

	node->disconnectAllInputs();
}

void Context::initRecursisve( const NodeRef &node )
{
	if( ! node )
		return;

	for( auto &in : node->getInputs() )
		initRecursisve( in.second );

	node->configureConnections();
}

void Context::uninitRecursisve( const NodeRef &node )
{
	if( ! node )
		return;

	for( auto &in : node->getInputs() )
		uninitRecursisve( in.second );

	node->uninitializeImpl();
}

void Context::addAutoPulledNode( const NodeRef &node )
{
	mAutoPulledNodes.insert( node );
	mAutoPullRequired = true;
	mAutoPullCacheDirty = true;

	// if not done already, allocate a buffer for auto-pulling that is large enough for stereo processing
	size_t framesPerBlock = getFramesPerBlock();
	if( mAutoPullBuffer.getNumFrames() < framesPerBlock )
		mAutoPullBuffer.setSize( framesPerBlock, 2 );
}

void Context::removeAutoPulledNode( const NodeRef &node )
{
	size_t result = mAutoPulledNodes.erase( node );
	CI_ASSERT( result );

	mAutoPullCacheDirty = true;
	if( mAutoPulledNodes.empty() )
		mAutoPullRequired = false;
}

void Context::processAutoPulledNodes()
{
	if( ! mAutoPullRequired )
		return;

	for( Node *node : getAutoPulledNodes() ) {
		mAutoPullBuffer.setNumChannels( node->getNumChannels() );
		node->pullInputs( &mAutoPullBuffer );
	}
}

const std::vector<Node *>& Context::getAutoPulledNodes()
{
	if( mAutoPullCacheDirty ) {
		mAutoPullCache.clear();
		for( const NodeRef &node : mAutoPulledNodes )
			mAutoPullCache.push_back( node.get() );
	}
	return mAutoPullCache;
}

namespace {

void printRecursive( const NodeRef &node, size_t depth )
{
	if( ! node )
		return;
	for( size_t i = 0; i < depth; i++ )
		app::console() << "-- ";

	string channelMode;
	switch( node->getChannelMode() ) {
		case Node::ChannelMode::SPECIFIED: channelMode = "specified"; break;
		case Node::ChannelMode::MATCHES_INPUT: channelMode = "matches input"; break;
		case Node::ChannelMode::MATCHES_OUTPUT: channelMode = "matches output"; break;
	}

	app::console() << node->getName() << "\t[ " << ( node->isEnabled() ? "enabled" : "disabled" );
	app::console() << ", ch: " << node->getNumChannels();
	app::console() << ", ch mode: " << channelMode;
	app::console() << ", " << ( node->getProcessInPlace() ? "in-place" : "sum" );
	app::console() << " ]" << endl;

	for( const auto &in : node->getInputs() )
		printRecursive( in.second, depth + 1 );
};

} // anonymous namespace

void Context::printGraph()
{
	app::console() << "-------------- Graph configuration: --------------" << endl;
	printRecursive( getOutput(), 0 );
	for( const auto& node : mAutoPulledNodes )
		printRecursive( node, 0 );
	app::console() << "--------------------------------------------------" << endl;
}


} } // namespace cinder::audio2