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

#include "cinder/audio2/Device.h"
#include "cinder/audio2/Exception.h"
#include "cinder/audio2/Buffer.h"

#include <boost/logic/tribool.hpp>

#include <memory>
#include <atomic>
#include <map>

namespace cinder { namespace audio2 {

typedef std::shared_ptr<class Context>			ContextRef;
typedef std::shared_ptr<class Node>				NodeRef;
typedef std::shared_ptr<class NodeMixer>		NodeMixerRef;

class Node : public std::enable_shared_from_this<Node> {
  public:
	typedef std::map<size_t, std::shared_ptr<Node> >		InputsContainerT;		//! input bus, strong reference to this
	typedef std::map<size_t, std::weak_ptr<Node> >			OutputsContainerT;		//! output bus for this node, weak reference to this

	enum ChannelMode {
		SPECIFIED,		//! Number of channels has been specified by user or is non-settable.
		MATCHES_INPUT,	//! Node matches it's channels with it's input.
		MATCHES_OUTPUT	//! Node matches it's channels with it's output.
	};

	struct Format {
		Format() : mChannels( 0 ), mChannelMode( ChannelMode::MATCHES_INPUT ), mAutoEnable( boost::logic::indeterminate ) {}

		Format& channels( size_t ch )							{ mChannels = ch; return *this; }
		Format& channelMode( ChannelMode mode )					{ mChannelMode = mode; return *this; }
		//! Whether or not the Node will be auto-enabled when connection changes occur.  Default is false for base \a Node class, although sub-classes may choose a different default.
		Format& autoEnable( boost::tribool tb = true )			{ mAutoEnable = tb; return *this; }

		size_t			getChannels() const						{ return mChannels; }
		ChannelMode		getChannelMode() const					{ return mChannelMode; }
		boost::tribool	getAutoEnable() const					{ return mAutoEnable; }

	protected:
		size_t			mChannels;
		ChannelMode		mChannelMode;
		boost::tribool	mAutoEnable;
	};

	virtual ~Node();

	std::string virtual getTag()				{ return "Node"; }

	//! Called before audio buffers need to be used. There is always a valid Context at this point.
	virtual void initialize()	{}
	//! Called once the contents of initialize are no longer relevant, i.e. destruction or the connections have changed
	virtual void uninitialize()	{}

	//! Returns the \a Context associated with this \a Node. \note Cannot be called from within a \a Node's constructor. Use initialize instead.
	ContextRef getContext() const				{ return mContext.lock(); }

	//! Enables this Node for processing. Same as setEnabled( true ).
	virtual void start()		{ mEnabled = true; }
	//! Disables this Node for processing. Same as setEnabled( false ).
	virtual void stop()			{ mEnabled = false; }
	//! Sets whether this Node is enabled for processing or not.
	void setEnabled( bool b = true );
	//! Returns whether this Node is enabled for processing or not.
	bool isEnabled() const						{ return mEnabled; }

	//! Connects this Node to \a dest on bus \a outputBus (default = 0). \a dest then references this Node as an input on \a inputBus (default = 0).
	virtual const NodeRef& connect( const NodeRef &dest, size_t outputBus = 0, size_t inputBus = 0 );
	//! Connects this Node to \a dest on the first available output bus. \a dest then references this Node as an input on the first available input bus.
	virtual const NodeRef& addConnection( const NodeRef &dest );
	//! Disconnects this Node from the output Node located at bus \a outputBus.
	virtual void disconnect( size_t outputBus = 0 );
	//! Disconnects this Node from all outputs.
	virtual void disconnectAllOutputs();
	//! Disconnects all of this Node's inputs.
	virtual void disconnectAllInputs();
	//! Returns the number of inputs connected to this Node.
	size_t getNumConnectedInputs() const;
	//! Returns the number of outputs this Node is connected to.
	size_t getNumConnectedOutputs() const;

	size_t		getNumChannels() const			{ return mNumChannels; }
	ChannelMode getChannelMode() const			{ return mChannelMode; }
	size_t		getMaxNumInputChannels() const;

	//! Sets whether this Node is automatically enabled / disabled when connected
	void	setAutoEnabled( bool b = true )		{ mAutoEnabled = b; }
	//! Returns whether this Node is automatically enabled / disabled when connected
	bool	isAutoEnabled() const				{ return mAutoEnabled; }

	//! Default implementation returns true if numChannels match our format
	virtual bool supportsInputNumChannels( size_t numChannels )	{ return mNumChannels == numChannels; }
	//! Override to perform custom processing on \t buffer
	virtual void process( Buffer *buffer )	{}
	//! Called prior to process(), override to control how this Node manages input channel mixing, summing and / or processing. The processed samples must eventually be in \t destBuffer (will be used in-place if possible).
	virtual void pullInputs( Buffer *destBuffer );

	// TODO: consider doing custom iterators and hiding these container types
	InputsContainerT& getInputs()			{ return mInputs; }
	OutputsContainerT& getOutputs()			{ return mOutputs; }

	//! Returns whether this Node is in an initialized state and is capabale of processing audio.
	bool isInitialized() const					{ return mInitialized; }
	//! Returns whether this Node will process audio with an in-place Buffer.
	bool getProcessInPlace() const				{ return mProcessInPlace; }

	// TODO: make this protected if possible (or better yet, not-accessible)
//	const Buffer *getInternalBuffer() const		{ return &mInternalBuffer; }
	const Buffer *getInternalBuffer() const		{ return &mSummingBuffer; }

  protected:
	Node( const Format &format );

	//! Stores \a input at bus \a inputBus, replacing any Node currently existing there. Stores this Node at input's output bus \a outputBus. Returns whether a new connection was made or not.
	//! \note Should be called on a non-audio thread and synchronized with the Context's mutex.
	virtual void connectInput( const NodeRef &input, size_t bus );
	virtual void disconnectInput( const NodeRef &input );
	virtual void disconnectOutput( const NodeRef &output );

	virtual void configureConnections();
	void setupProcessWithSumming();
	void notifyConnectionsDidChange();

	//! Only Node subclasses can specify num channels directly - users specify via Format at construction time
	void setNumChannels( size_t numChannels );
	bool checkInput( const NodeRef &input );
	size_t getFirstAvailableOutputBus();
	size_t getFirstAvailableInputBus();

	void initializeImpl();
	void uninitializeImpl();

	std::atomic<bool>		mEnabled;
	InputsContainerT		mInputs;
	OutputsContainerT		mOutputs;

	bool					mInitialized;
	bool					mAutoEnabled;
	bool					mProcessInPlace;
	size_t					mNumChannels;
	ChannelMode				mChannelMode;
	uint64_t				mLastProcessedFrame;

	BufferDynamic			mInternalBuffer, mSummingBuffer;

  private:
	// noncopyable
	Node( Node const& );
	Node& operator=( Node const& );

	void setContext( const ContextRef &context )	{ mContext = context; }

	std::weak_ptr<Context>	mContext;
	friend class Context;
};

//! a Node that can be pulled without being connected to any outputs.
class NodeAutoPullable : public Node {
  public:
	virtual ~NodeAutoPullable();
	virtual const NodeRef& connect( const NodeRef &dest, size_t outputBus = 0, size_t inputBus = 0 ) override;
	virtual void connectInput( const NodeRef &input, size_t bus )	override;
	virtual void disconnectInput( const NodeRef &input )			override;

  protected:
	NodeAutoPullable( const Format &format );
	void updatePullMethod();

	bool mIsPulledByContext;
};

//! Convenience routine for finding the first downstream \a Node of type \a NodeT (traverses outputs).
template <typename NodeT>
static std::shared_ptr<NodeT> findFirstDownstreamNode( NodeRef node )
{
	if( ! node )
		return;

	for( auto &out : node->getOutputs() ) {
		auto output = out.second.lock();
		if( ! output )
			continue;

		auto castedNode = std::dynamic_pointer_cast<NodeT>( output );
		if( castedNode )
			return castedNode;

		return findFirstDownstreamNode<NodeT>( output );
	}

	return std::shared_ptr<NodeT>();
}

//! Convenience routine for finding the first upstream \a Node of type \a NodeT (traverses inputs).
// TODO: pass as const&, for this and downstream
template <typename NodeT>
static std::shared_ptr<NodeT> findFirstUpstreamNode( NodeRef node )
{
	CI_ASSERT( node );

	for( auto &in : node->getInputs() ) {
		auto& input = in.second;
		auto castedNode = std::dynamic_pointer_cast<NodeT>( input );
		if( castedNode )
			return castedNode;

		return findFirstUpstreamNode<NodeT>( input );
	}

	return std::shared_ptr<NodeT>();
}

//! Convenience class that pushes and pops a \a Node's current enabled state.
struct SaveNodeEnabledState {
	SaveNodeEnabledState( const NodeRef &node ) : mNode( node )
	{
		mEnabled = ( mNode ? mNode->isEnabled() : false );
	}
	~SaveNodeEnabledState()
	{
		if( mNode )
			mNode->setEnabled( mEnabled );
	}
  private:
	NodeRef mNode;
	bool mEnabled;
};

} } // namespace cinder::audio2
