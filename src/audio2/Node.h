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
	typedef std::map<size_t, std::shared_ptr<Node> >		InputsContainerT;		//! bus, strong reference to this
	typedef std::multimap<size_t, std::weak_ptr<Node> >		OutputsContainerT;		//! output's bus for this node, weak reference to this

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

	// TODO: solve these ambiguities, it isn't clear that one version adds and one sets at call sight...
	//	- may remove these in favor of operator>>
	virtual const NodeRef& connect( const NodeRef &dest )					{ dest->addInput( shared_from_this() ); return dest; }
	virtual const NodeRef& connect( const NodeRef &dest, size_t bus )		{ dest->setInput( shared_from_this(), bus ); return dest; }

	virtual void disconnect( size_t bus = 0 );

	//! Insert \a input in first available bus, append if necessary.
	virtual void addInput( const NodeRef &input );
	//! Sets \a input at \a bus, replacing any Node currently existing there.
	virtual void setInput( const NodeRef &input, size_t bus = 0 );

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

//	NodeRef getOutput( size_t bus = 0 )	const		{ return mOutputs[bus].lock(); }
//	void	setOutput( const NodeRef &output )		{ mOutput = output; }

	bool isInitialized() const					{ return mInitialized; }

	bool isEnabled() const						{ return mEnabled; }
	void setEnabled( bool b = true );

	bool getProcessInPlace() const				{ return mProcessInPlace; }

	size_t getNumInputs() const;

	// TODO: make this protected if possible (or better yet, not-accessible)
//	const Buffer *getInternalBuffer() const		{ return &mInternalBuffer; }
	const Buffer *getInternalBuffer() const		{ return &mSummingBuffer; }

  protected:

	virtual void configureConnections();
	void setupProcessWithSumming();

	//! Only Node subclasses can specify num channels directly - users specify via Format at construction time
	void setNumChannels( size_t numChannels );
	bool checkInput( const NodeRef &input );
	size_t getFirstAvailableBus();

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
	NodeAutoPullable( const Format &format );

	virtual void	addInput( const NodeRef &input )						override;
	virtual void	setInput( const NodeRef &input, size_t bus )			override;
	virtual const	NodeRef& connect( const NodeRef &dest )					override;
	virtual const	NodeRef& connect( const NodeRef &dest, size_t bus )		override;
	virtual void	disconnect( size_t bus )								override;

  protected:
	void updatePullMethod();

	bool mIsPulledByContext;
};

//! TODO: move or remove
class NodeMixer : public Node {
  public:
	NodeMixer( const Format &format = Format() ) : Node( format ), mMaxNumBusses( 10 ) {}
	virtual ~NodeMixer() {}

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

////! Convenience routine for finding the first downstream \a Node of type \a NodeT (traverses outputs).
//template <typename NodeT>
//static std::shared_ptr<NodeT> findFirstDownstreamNode( NodeRef node )
//{
//	while( node ) {
//		auto castedNode = std::dynamic_pointer_cast<NodeT>( node );
//		if( castedNode )
//			return castedNode;
//		node = node->getOutput();
//	}
//	return std::shared_ptr<NodeT>();
//}
//
////! Convenience routine for finding the first upstream \a Node of type \a NodeT (traverses inputs).
//// FIXME: account for multiple inputs
//// TODO: pass as const&, but requires switching to recursive traversal
//template <typename NodeT>
//static std::shared_ptr<NodeT> findFirstUpstreamNode( NodeRef node )
//{
//	while( node ) {
//		auto castedNode =std::dynamic_pointer_cast<NodeT>( node );
//		if( castedNode )
//			return castedNode;
//		else if( node->getInputs().empty() )
//			break;
//
//		node = node->getInputs().front();
//	}
//	return std::shared_ptr<NodeT>();
//}

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
	if( ! node )
		return;

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
