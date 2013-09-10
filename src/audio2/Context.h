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

#pragma once

#include "audio2/Node.h"

#include <mutex>

namespace cinder { namespace audio2 {

class DeviceManager;

class Context : public std::enable_shared_from_this<Context> {
  public:
	virtual ~Context();

	//! Returns a platform-specific \a Context. If none is availble, returns an empty \a ContextRef.
	static ContextRef			create();

	virtual LineOutNodeRef		createLineOut( DeviceRef device = Device::getDefaultOutput(), const Node::Format &format = Node::Format() ) = 0;
	virtual LineInNodeRef		createLineIn( DeviceRef device = Device::getDefaultInput(), const Node::Format &format = Node::Format() ) = 0;

	template<typename NodeT>
	std::shared_ptr<NodeT>		makeNode( NodeT *node );

	//! Returns the platform-specific \a DeviceManager singleton instance. If none is available, returns \a nullptr.
	static DeviceManager* deviceManager();

	virtual void setRoot( RootNodeRef root );

	//! If the root has not already been set, it is the default LineOutNode
	virtual RootNodeRef getRoot();
	virtual void start();
	virtual void stop();
	//! start / stop the graph via bool
	void setEnabled( bool enabled = true );

	bool isEnabled() const		{ return mEnabled; }

	//! Disconnect all Node's related by this Context
	virtual void disconnectAllNodes();
	//! Called by \a node when it's connections have changed, default implementation is empty.
	virtual void connectionsDidChange( const NodeRef &node ) {} 

	size_t getSampleRate()					{ return getRoot()->getSampleRate(); }
	size_t getFramesPerBlock()				{ return getRoot()->getFramesPerBlock(); }
	size_t getElapsedFrames()				{ return getRoot()->getElapsedFrames(); }

	std::mutex& getMutex()					{ return mMutex; }

  protected:
	Context() : mEnabled( false ) {}

	void startRecursive( const NodeRef &node );
	void stopRecursive( const NodeRef &node );
	void disconnectRecursive( const NodeRef &node );
	void uninitRecursisve( const NodeRef &node );

	RootNodeRef		mRoot;
	std::mutex		mMutex;
	bool			mEnabled;

	static std::unique_ptr<DeviceManager> sDeviceManager;
};

template<typename NodeT>
std::shared_ptr<NodeT> Context::makeNode( NodeT *node )
{
	std::shared_ptr<NodeT> result( node );
	result->setContext( shared_from_this() );
	return result;
}

} } // namespace cinder::audio2