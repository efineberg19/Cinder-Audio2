#pragma once

#include "audio2/Device.h"
#include "audio2/Buffer.h"

#include <memory>
#include <vector>

namespace audio2 {

typedef std::shared_ptr<class Graph> GraphRef;
typedef std::shared_ptr<class Node> NodeRef;
typedef std::weak_ptr<class Node> NodeWeakRef;

typedef std::shared_ptr<class MixerNode> MixerNodeRef;
typedef std::shared_ptr<class RootNode> RootNodeRef;
typedef std::shared_ptr<class OutputNode> OutputNodeRef;
typedef std::shared_ptr<class TapNode> TapNodeRef;

class Node : public std::enable_shared_from_this<Node> {
  public:
	virtual ~Node();

	struct Format {
		Format() : mSampleRate( 0 ), mNumChannels( 0 ), mWantsDefaultFormatFromParent( false ), mBufferFormat( Buffer::Format::NonInterleaved ) // TODO: decide how to properly set this format per platform
		{}

		virtual bool isComplete() const	{ return ( mSampleRate && mNumChannels ); }

		size_t	getSampleRate() const	{ return mSampleRate; }
		void	setSampleRate( size_t sampleRate )	{ mSampleRate = sampleRate; }
		size_t	getNumChannels() const	{ return mNumChannels; }
		void	setNumChannels( size_t numChannels )	{ mNumChannels = numChannels; }
		bool	wantsDefaultFormatFromParent() const	{ return mWantsDefaultFormatFromParent; }
		void	setWantsDefaultFormatFromParent( bool b = true )	{ mWantsDefaultFormatFromParent = b; }

		const Buffer::Format& getBufferFormat() const { return mBufferFormat; }
		void	setBufferFormat( const Buffer::Format& format )	{ mBufferFormat = format; }
		
  private:
		size_t mSampleRate, mNumChannels;
		bool mWantsDefaultFormatFromParent;
		Buffer::Format			mBufferFormat;
	};

	virtual void initialize()	{ mInitialized = true; }
	virtual void uninitialize()	{ mInitialized = false; }
	virtual void start()		{}
	virtual void stop()			{}

	virtual NodeRef connect( NodeRef source );

	//! Default implementation returns true if samplerate and numChannels match our format
	virtual bool supportsSourceFormat( const Format &sourceFormat ) const;

	virtual void render( Buffer *buffer )	{}

	std::vector<NodeRef>& getSources()			{ return mSources; }
	NodeRef getParent()							{ return mParent.lock(); }
	void setParent( NodeRef parent )			{ mParent = parent; }

	Format& getFormat()	{ return mFormat; }

	//! Default implementation returns the format for the first source
	virtual const Format& getSourceFormat();

	const std::string& getTag()	const	{ return mTag; }

	bool isInitialized() const	{ return mInitialized; }

  protected:
	Node() : mInitialized( false )
	{}

	std::vector<NodeRef>	mSources;
	NodeWeakRef				mParent;
	Format					mFormat;
	bool					mInitialized;
	std::string				mTag;

  private:
	  Node( Node const& );
	  Node& operator=( Node const& );
};

class RootNode : public Node {
  public:
	RootNode() : Node() { mSources.resize( 1 ); }
	virtual ~RootNode() {}

	virtual size_t getBlockSize() const = 0;
};

class OutputNode : public RootNode {
  public:

	// ???: device param here necessary?
	OutputNode( DeviceRef device ) : RootNode() {}
	virtual ~OutputNode() {}

	virtual DeviceRef getDevice() = 0;

  protected:
};

class RingBuffer;

class TapNode : public Node {
  public:
	TapNode( size_t bufferSize = 1024 );
	virtual ~TapNode();

	const float* getChannel( size_t ch = 0 );
	const Buffer& getBuffer();

	virtual void initialize() override;
	virtual void render( Buffer *buffer ) override;

  private:
	std::vector<std::unique_ptr<RingBuffer> > mRingBuffers; // TODO: make this one continuous buffer so it better matches audio::Buffer
	Buffer mCopiedBuffer;
	size_t mBufferSize;
};

class MixerNode : public Node {
  public:
	MixerNode() : Node(), mMaxNumBusses( 10 ) { mSources.resize( mMaxNumBusses ); }
	virtual ~MixerNode() {}

	virtual NodeRef connect( NodeRef source ) override;
	virtual NodeRef connect( NodeRef source, size_t bus );

	//! returns the number of connected busses.
	virtual size_t getNumBusses() = 0;
	virtual void setNumBusses( size_t count ) = 0;	// ???: does this make sense now? should above be getNumActiveBusses?
	virtual size_t getMaxNumBusses()	{ return mMaxNumBusses; }
	virtual void setMaxNumBusses( size_t count ) = 0;
	virtual bool isBusEnabled( size_t bus ) = 0;
	virtual void setBusEnabled( size_t bus, bool enabled = true ) = 0;
	virtual void setBusVolume( size_t bus, float volume ) = 0;
	virtual float getBusVolume( size_t bus ) = 0;
	virtual void setBusPan( size_t bus, float pan ) = 0;
	virtual float getBusPan( size_t bus ) = 0;

protected:
	size_t mMaxNumBusses;
};

class Graph {
  public:
	virtual ~Graph();

	virtual void initialize();
	virtual void uninitialize();
	virtual void setRoot( RootNodeRef root )	{ mRoot = root; }
	virtual RootNodeRef getRoot() const	{ return mRoot; }
	virtual void start();
	virtual void stop();

	bool isInitialized() const	{ return mInitialized; }
	bool isRunning() const		{ return mRunning; }

  protected:
	Graph() : mInitialized( false ), mRunning( false ) {}

	virtual void start( NodeRef node );
	virtual void stop( NodeRef node );


	RootNodeRef		mRoot;
	bool			mInitialized, mRunning;
};

} // namespace audio2