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

#include "audio2/Device.h"
#include "audio2/Buffer.h"
#include "audio2/Atomic.h"

#include <memory>
#include <vector>

namespace audio2 {

typedef std::shared_ptr<class Context> ContextRef;
typedef std::shared_ptr<class Node> NodeRef;
typedef std::shared_ptr<class GeneratorNode> GeneratorNodeRef;
typedef std::shared_ptr<class RootNode> RootNodeRef;
typedef std::shared_ptr<class LineOutNode> LineOutNodeRef;
typedef std::shared_ptr<class LineInNode> LineInNodeRef;
typedef std::shared_ptr<class MixerNode> MixerNodeRef;
typedef std::shared_ptr<class FilePlayerNode> FilePlayerNodeRef;

class Node : public std::enable_shared_from_this<Node> {
  public:
	virtual ~Node();

	struct Format {
		Format() : mChannels( 0 ), mWantsDefaultFormatFromOutput( false ) {}
		
		Format& channels( size_t ch )							{ mChannels = ch; return *this; }
		Format& wantsDefaultFormatFromOutput( bool b = true )	{ mWantsDefaultFormatFromOutput = b; return *this; }

		size_t	getChannels() const								{ return mChannels; }
		bool	getWantsDefaultFormatFromOutput() const			{ return mWantsDefaultFormatFromOutput; }

	  private:
		size_t mChannels;
		bool mWantsDefaultFormatFromOutput;
	};

	virtual void initialize()	{ mInitialized = true; }
	virtual void uninitialize()	{ mInitialized = false; }
	virtual void start()		{ mEnabled = true; }
	virtual void stop()			{ mEnabled = false; }

	virtual NodeRef connect( NodeRef dest );
	virtual NodeRef connect( NodeRef dest, size_t bus );

	virtual void disconnect( size_t bus = 0 );

	//! insert in first available slot or append the node if called without a bus number.
	virtual void setInput( NodeRef input );
	virtual void setInput( NodeRef input, size_t bus );

	size_t	getNumChannels() const	{ return mNumChannels; }

	bool	isNumChannelsUnspecified() const	{ return mNumChannelsUnspecified; }
	bool	getWantsDefaultFormatFromOutput() const			{ return mWantsDefaultFormatFromOutput; }

//	void	setWantsDefaultFormatFromOutput( bool b = true )	{ mWantsDefaultFormatFromOutput = b; }

	const Buffer::Layout& getBufferLayout() const { return mBufferLayout; }

	//! controls whether the owning Context automatically enables / disables this Node
	bool	isAutoEnabled() const				{ return mAutoEnabled; }
	void	setAutoEnabled( bool b = true )		{ mAutoEnabled = b; }

	//! Default implementation returns true if numChannels match our format
	virtual bool supportsSourceNumChannels( size_t numChannels ) const	{ return mNumChannels == numChannels; }

	//! If required Format properties are missing, fill in params from output
	virtual void fillFormatParamsFromOutput();

	//! If required Format properties are missing, fill in params from first input
	virtual void fillFormatParamsFromInput();

	//! Override to perform processing or analysis on \t buffer
	virtual void process( Buffer *buffer )	{}

	//! Default implementation pulls samples from all inputs and sums the result.
	virtual void pullInputs();

	std::vector<NodeRef>& getInputs()			{ return mInputs; }
	NodeRef getOutput()	const					{ return mOutput.lock(); }
	void setOutput( NodeRef output )			{ mOutput = output; }

	ContextRef getContext() const				{ return mContext.lock(); }
	void setContext( ContextRef context )		{ mContext = context; }

	const std::string& getTag()	const	{ return mTag; }

	bool isInitialized() const	{ return mInitialized; }
	bool isConnected() const	{ return mConnected; }
	bool isEnabled() const		{ return mEnabled; }

	void setEnabled( bool b = true );

	// TODO: make this protected if possible
	const Buffer *getInternalBuffer() const	{ return &mInternalBuffer; }

  protected:
	Node( const Format &format );

	//! If required Format properties are missing, fill in from \a otherFormat
	virtual void fillFormatParamsFromNode( const NodeRef &otherNode );

	//! Only Node subclasses can specify num channels directly - users specify via Format at construction time
	void	setNumChannels( size_t numChannels )	{ mNumChannels = numChannels; mNumChannelsUnspecified = false; }

	std::vector<NodeRef>	mInputs;
	std::weak_ptr<Node>		mOutput;
	std::weak_ptr<Context>	mContext;
	bool					mInitialized;
	std::atomic<bool>		mConnected, mEnabled;
	std::string				mTag;

	size_t mNumChannels;
	bool mWantsDefaultFormatFromOutput, mNumChannelsUnspecified;
	bool mAutoEnabled;
	Buffer::Layout			mBufferLayout; // TODO: remove, use mInternalBuffer.getLayout()
	Buffer					mInternalBuffer;

  private:
	  Node( Node const& );
	  Node& operator=( Node const& );
};

class RootNode : public Node {
  public:
	RootNode( const Format &format = Format() ) : Node( format ) {}
	virtual ~RootNode() {}

	// TODO: need to decide where user sets the samplerate / blocksize - on RootNode or Context?
	// - this is still needed to determine a default
	// - also RootNode has to agree with the sampleate - be it output out, file out, whatever
	virtual size_t getSampleRate() = 0;
	virtual size_t getNumFramesPerBlock() = 0;

  private:
	// RootNode subclasses cannot connect to anything else
	NodeRef connect( NodeRef dest ) override				{ return NodeRef(); }
	NodeRef connect( NodeRef dest, size_t bus ) override	{ return NodeRef(); }
};

class LineOutNode : public RootNode {
  public:

	// ???: device param here necessary?
	LineOutNode( DeviceRef device, const Format &format = Format() ) : RootNode( format ) {
		setAutoEnabled();
	}
	virtual ~LineOutNode() {}

	virtual DeviceRef getDevice() = 0;

	size_t getSampleRate()			{ return getDevice()->getSampleRate(); }
	size_t getNumFramesPerBlock()	{ return getDevice()->getNumFramesPerBlock(); }

  protected:
};

class MixerNode : public Node {
  public:
	MixerNode( const Format &format = Format() ) : Node( format ), mMaxNumBusses( 10 ) { mInputs.resize( mMaxNumBusses ); }
	virtual ~MixerNode() {}

	//! returns the number of connected busses.
	virtual size_t getNumBusses() = 0;
	virtual void setNumBusses( size_t count ) = 0;	// ???: does this make sense now? should above be getNumActiveBusses?
	virtual size_t getMaxNumBusses()	{ return mMaxNumBusses; }
	virtual void setMaxNumBusses( size_t count ) = 0;
	virtual void setBusVolume( size_t bus, float volume ) = 0;
	virtual float getBusVolume( size_t bus ) = 0;
	virtual void setBusPan( size_t bus, float pan ) = 0;
	virtual float getBusPan( size_t bus ) = 0;

	// TODO: decide whether it is appropriate for MixerNode to enable / disable busses
	// - Node's can be enabled / disabled now)
	// - this should probably be internal - an impl such as MixerNodeAudioUnit will
	//	 disable all busses that doesn't have a corresponding source Node hooked up to them
	virtual bool isBusEnabled( size_t bus ) = 0;
	virtual void setBusEnabled( size_t bus, bool enabled = true ) = 0;

protected:
	// TODO: Because busses can be expanded, the naming is off:
	//		- mMaxNumBusses should be mNumBusses, there is no max
	//			- so there is getNumBusses() / setNumBusses()
	//		- there can be 'holes', slots in mInputs that are not used
	//		- getNumActiveBusses() returns number of used slots
	size_t mMaxNumBusses;
};

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


	size_t getSampleRate() const			{ return mSampleRate; }
	size_t getNumFramesPerBlock() const		{ return mNumFramesPerBlock; }

  protected:
	Context() : mInitialized( false ), mEnabled( false ) {}

	virtual void start( NodeRef node );
	virtual void stop( NodeRef node );

	RootNodeRef		mRoot;
	bool			mInitialized, mEnabled;
	size_t			mSampleRate, mNumFramesPerBlock;
};

} // namespace audio2