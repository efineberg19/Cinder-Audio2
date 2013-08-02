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

namespace audio2 {

class Context : public std::enable_shared_from_this<Context> {
  public:
	virtual ~Context();

	virtual ContextRef			createContext() = 0;
	virtual MixerNodeRef		createMixer( const Node::Format &format = Node::Format() ) = 0;
	virtual LineOutNodeRef		createLineOut( DeviceRef device = Device::getDefaultOutput(), const Node::Format &format = Node::Format() ) = 0;
	virtual LineInNodeRef		createLineIn( DeviceRef device = Device::getDefaultInput(), const Node::Format &format = Node::Format() ) = 0;

	static Context* instance();

	virtual void initialize();
	virtual void uninitialize();
	virtual void setRoot( RootNodeRef root )	{ mRoot = root; }

	//! If the root has not already been set, it is the default LineOutNode
	virtual RootNodeRef getRoot();
	virtual void start();
	virtual void stop();


	bool isInitialized() const	{ return mInitialized; }

	bool isEnabled() const		{ return mEnabled; }

	//! convenience method to start / stop the graph via bool
	void setEnabled( bool enabled = true );

	void disconnectAllNodes();

	size_t getSampleRate() const			{ return mSampleRate; }
	size_t getNumFramesPerBlock() const		{ return mNumFramesPerBlock; }

  protected:
	Context() : mInitialized( false ), mEnabled( false ) {}

	void startRecursive( const NodeRef &node );
	void stopRecursive( const NodeRef &node );
	void disconnectRecursive( const NodeRef &node );

	RootNodeRef		mRoot;
	bool			mInitialized, mEnabled;
	size_t			mSampleRate, mNumFramesPerBlock;
};

} // namespace audio2