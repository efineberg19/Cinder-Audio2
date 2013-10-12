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
#include "audio2/NodeSource.h"
#include "audio2/audio.h"

#include "cinder/Cinder.h"

#if defined( CINDER_COCOA )
	#include "audio2/cocoa/ContextAudioUnit.h"
	#if defined( CINDER_MAC )
		#include "audio2/cocoa/DeviceManagerCoreAudio.h"
	#else // CINDER_COCOA_TOUCH
		#include "audio2/cocoa/DeviceManagerAudioSession.h"
	#endif
#elif defined( CINDER_MSW )
	#include "audio2/msw/ContextXAudio.h"
	#include "audio2/msw/DeviceManagerWasapi.h"
#endif

using namespace std;

namespace cinder { namespace audio2 {

std::shared_ptr<Context>		Context::sHardwareContext;
std::unique_ptr<DeviceManager>	Context::sDeviceManager;

Context* Context::hardwareInstance()
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
	getTarget()->start();
}

void Context::stop()
{
	if( ! mEnabled )
		return;

	mEnabled = false;
	getTarget()->stop();
}

void Context::disconnectAllNodes()
{
	if( mEnabled )
		stop();
	
	disconnectRecursive( mTarget );
}

void Context::setEnabled( bool enabled )
{
	if( enabled )
		start();
	else
		stop();
}

void Context::setTarget( const NodeTargetRef &target )
{
	mTarget = target;
}

const NodeTargetRef& Context::getTarget()
{
	if( ! mTarget )
		mTarget = createLineOut();
	return mTarget;
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
	for( auto& input : node->getInputs() )
		disconnectRecursive( input );

	node->disconnect();
}

void Context::initRecursisve( const NodeRef &node )
{
	if( ! node )
		return;

	for( const NodeRef &input : node->getInputs() )
		initRecursisve( input );

	node->configureConnections();
}

void Context::uninitRecursisve( const NodeRef &node )
{
	if( ! node )
		return;

	for( const NodeRef &input : node->getInputs() )
		uninitRecursisve( input );

	node->uninitializeImpl();
}

} } // namespace cinder::audio2