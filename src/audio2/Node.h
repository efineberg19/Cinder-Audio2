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

namespace cinder { namespace audio2 {

typedef std::shared_ptr<class Context> ContextRef;
typedef std::shared_ptr<class Node> NodeRef;
typedef std::shared_ptr<class NodeSource> NodeSourceRef;
typedef std::shared_ptr<class NodeTarget> NodeTargetRef;
typedef std::shared_ptr<class LineOutNode> LineOutNodeRef;
typedef std::shared_ptr<class LineInNode> LineInNodeRef;
typedef std::shared_ptr<class MixerNode> MixerNodeRef;
typedef std::shared_ptr<class FilePlayerNode> FilePlayerNodeRef;

class Node : public std::enable_shared_from_this<Node> {
  public:
	enum ChannelMode {
		SPECIFIED,		//! Number of channels has been specified by user or is non-settable.
		MATCHES_INPUT,	//! Node matches it's channels with it's input.
		MATCHES_OUTPUT	//! Node matches it's channels with it's output.
	};

	struct Format {
		Format() : mChannels( 0 ), mChannelMode( ChannelMode::MATCHES_INPUT ) {}

		Format& channels( size_t ch )							{ mChannels = ch; return *this; }
		Format& channelMode( ChannelMode mode )					{ mChannelMode = mode; return *this; }

		size_t	getChannels() const								{ return mChannels; }
		ChannelMode	getChannelMode() const						{ return mChannelMode; }

	protected:
		size_t mChannels;
		ChannelMode mChannelMode;
	};

	Node( const Format &format );
	virtual ~Node();

	std::string virtual getTag()				{ return "Node"; }

	//! Called before audio buffers need to be used. There is always a valid Context at this point.
	virtual void initialize()	{}
	//! Called once the contents of initialize are no longer relevant, i.e. destruction or the connections have changed
	virtual void uninitialize()	{}

	//! Returns the \a Context associated with this \a Node. \note Cannot be called from within a \a Node's constructor. Use initialize instead.
	ContextRef getContext() const				{ return mContext.lock(); }


	virtual void start()		{ mEnabled = true; }
	virtual void stop()			{ mEnabled = false; }

	virtual const NodeRef& connect( const NodeRef &dest );
	virtual const NodeRef& connect( const NodeRef &dest, size_t bus );

	virtual void disconnect( size_t bus = 0 );

	//! insert in first available slot or append the node if called without a bus number.
	// TODO: consider a more descriptive name, not really obvious this will append. addInput() ?
	virtual void setInput( const NodeRef &input );
	virtual void setInput( const NodeRef &input, size_t bus );

	bool isConnectedToInput( const NodeRef &input ) const;
	bool isConnectedToOutput( const NodeRef &output ) const;

	size_t	getNumChannels() const			{ return mNumChannels; }
	ChannelMode getChannelMode() const		{ return mChannelMode; }

	//! controls whether the owning Context automatically enables / disables this Node
	bool	isAutoEnabled() const				{ return mAutoEnabled; }
	void	setAutoEnabled( bool b = true )		{ mAutoEnabled = b; }

	//! Default implementation returns true if numChannels match our format
	virtual bool supportsSourceNumChannels( size_t numChannels ) const	{ return mNumChannels == numChannels; }
	//! Override to perform custom processing on \t buffer
	virtual void process( Buffer *buffer )	{}
	//! Override to control how this Node manages input processing. The processed samples must eventually be in \t outputBuffer (can be used in-place if possible).
	virtual void pullInputs( Buffer *outputBuffer );

	// TODO: it's probably a good idea to hide this structure
	std::vector<NodeRef>& getInputs()			{ return mInputs; }

	NodeRef getOutput()	const					{ return mOutput.lock(); }
	void setOutput( const NodeRef &output )		{ mOutput = output; }

	bool isInitialized() const					{ return mInitialized; }

	bool isEnabled() const						{ return mEnabled; }
	void setEnabled( bool b = true );

	bool getProcessInPlace() const				{ return mProcessInPlace; }

	size_t getNumInputs() const;

	// TODO: make this protected if possible
//	const Buffer *getInternalBuffer() const		{ return &mInternalBuffer; }
	const Buffer *getInternalBuffer() const		{ return &mSummingBuffer; }

  protected:

	virtual void configureConnections();
	virtual void submixBuffers( Buffer *destBuffer, const Buffer *sourceBuffer );

	void setProcessWithSumming();

	//! Only Node subclasses can specify num channels directly - users specify via Format at construction time
	void setNumChannels( size_t numChannels );
	bool checkInput( const NodeRef &input );

	void initializeImpl();
	void uninitializeImpl();

	std::vector<NodeRef>	mInputs;
	std::weak_ptr<Node>		mOutput;
	std::atomic<bool>		mEnabled;

	bool					mInitialized;
	bool					mAutoEnabled;
	bool					mProcessInPlace;
	size_t					mNumChannels;
	ChannelMode				mChannelMode;

	Buffer					mInternalBuffer, mSummingBuffer;

  private:
	Node( Node const& );
	Node& operator=( Node const& );

	void setContext( const ContextRef &context )	{ mContext = context; }

	std::weak_ptr<Context>	mContext;
	friend class Context;
};

class NodeTarget : public Node {
  public:
	NodeTarget( const Format &format = Format() ) : Node( format ) {}
	virtual ~NodeTarget() {}

	virtual size_t getSampleRate() = 0;
	virtual size_t getFramesPerBlock() = 0;

	//! Returns the total number of frames that have already been processed in the dsp loop.
	virtual uint64_t getNumProcessedFrames() = 0;

  private:
	// NodeTarget does not have outputs
	const NodeRef& connect( const NodeRef &dest ) override				{ return dest; }
	const NodeRef& connect( const NodeRef &dest, size_t bus ) override	{ return dest; }
};

class LineOutNode : public NodeTarget {
  public:

	// ???: device param here necessary?
	LineOutNode( const DeviceRef &device, const Format &format = Format() );
	virtual ~LineOutNode() {}

	const DeviceRef& getDevice() const		{ return mDevice; }

	size_t getSampleRate() override			{ return getDevice()->getSampleRate(); }
	size_t getFramesPerBlock() override		{ return getDevice()->getFramesPerBlock(); }

  protected:
	DeviceRef mDevice;
};

class MixerNode : public Node {
  public:
	MixerNode( const Format &format = Format() ) : Node( format ), mMaxNumBusses( 10 ) { mInputs.resize( mMaxNumBusses ); }
	virtual ~MixerNode() {}

	std::string virtual getTag()				{ return "MixerNode"; }

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

//! Helper routine for finding an upstream \a NodeT of a specific type (traverses outputs).
template <typename NodeT>
static std::shared_ptr<NodeT> findUpstreamNode( NodeRef node )
{
	while( node ) {
		auto castedNode = std::dynamic_pointer_cast<NodeT>( node );
		if( castedNode )
			return castedNode;
		node = node->getOutput();
	}
	return std::shared_ptr<NodeT>();
}

//! Helper routine for finding a downstream \a NodeT of a specific type (traverses inputs ).
// FIXME: account for multiple inputs
// TODO: pass as const&, but requires switching to recursive traversal
template <typename NodeT>
static std::shared_ptr<NodeT> findDownStreamNode( NodeRef node )
{
	while( node ) {
		auto castedNode =std::dynamic_pointer_cast<NodeT>( node );
		if( castedNode )
			return castedNode;
		else if( node->getInputs().empty() )
			break;

		node = node->getInputs().front();
	}
	return std::shared_ptr<NodeT>();
}

} } // namespace cinder::audio2
