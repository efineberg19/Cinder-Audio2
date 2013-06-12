#pragma once

#include "audio2/Device.h"
#include "audio2/Buffer.h"
//#include "audio2/GeneratorNode.h"

#include <memory>
#include <vector>

namespace audio2 {

class GeneratorNode;

typedef std::shared_ptr<class Context> ContextRef;
typedef std::shared_ptr<class Node> NodeRef;
typedef std::weak_ptr<class Node> NodeWeakRef;

typedef std::shared_ptr<class MixerNode> MixerNodeRef;
typedef std::shared_ptr<class RootNode> RootNodeRef;
typedef std::shared_ptr<class OutputNode> OutputNodeRef;
typedef std::shared_ptr<class TapNode> TapNodeRef;

typedef std::shared_ptr<class GeneratorNode> GeneratorNodeRef;
typedef std::shared_ptr<class InputNode> InputNodeRef;
typedef std::shared_ptr<class FileInputNode> FileInputNodeRef;

class Node : public std::enable_shared_from_this<Node> {
  public:
	virtual ~Node();

	struct Format {
		Format()
		: mSampleRate( 0 ), mNumChannels( 0 ), mWantsDefaultFormatFromParent( false ), mBufferFormat( Buffer::Format::NonInterleaved ), mAutoEnabled( false )
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

		//! controls whether the graph automatically enables / disables this Node
		bool	isAutoEnabled() const				{ return mAutoEnabled; }
		void	setAutoEnabled( bool b = true )		{ mAutoEnabled = b; }

  private:
		size_t mSampleRate, mNumChannels;
		bool mWantsDefaultFormatFromParent;
		bool mAutoEnabled;
		Buffer::Format			mBufferFormat;
	};

	virtual void initialize()	{ mInitialized = true; }
	virtual void uninitialize()	{ mInitialized = false; }
	virtual void start()		{}
	virtual void stop()			{}

	NodeRef connect( NodeRef dest );
	NodeRef connect( NodeRef dest, size_t bus );

	virtual void setSource( NodeRef source );
	virtual void setSource( NodeRef source, size_t bus );

	//! Default implementation returns true if samplerate and numChannels match our format
	virtual bool supportsSourceFormat( const Format &sourceFormat ) const;

	virtual void process( Buffer *buffer )	{}

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

	// TODO: consider how to best inform the user you cannot connect anything after a RootNode.
	// - could make this private, but it can still be called by typecasting to Node first, and that may also be more confusing than throwing
//	NodeRef setSource( NodeRef source ) override	{ throw AudioContextExc( "RootNode's cannot connect" ); }

};

class OutputNode : public RootNode {
  public:

	// ???: device param here necessary?
	OutputNode( DeviceRef device ) : RootNode() {
		mFormat.setAutoEnabled();
	}
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
	virtual void process( Buffer *buffer ) override;

  private:
	std::vector<std::unique_ptr<RingBuffer> > mRingBuffers; // TODO: make this one continuous buffer so it better matches audio::Buffer
	Buffer mCopiedBuffer;
	size_t mBufferSize;
};

// TODO: Because busses can be expanded, the naming is off:
//		- mMaxNumBusses should be mNumBusses, there is no max
//			- so there is getNumBusses() / setNumBusses()
//		- there can be 'holes', slots in mSources that are not used
//		- getNumActiveBusses() returns number of used slots

class MixerNode : public Node {
  public:
	MixerNode() : Node(), mMaxNumBusses( 10 ) { mSources.resize( mMaxNumBusses ); }
	virtual ~MixerNode() {}

	using Node::setSource;
	//! Mixers will append the node if setSource() is called without a bus number.
	virtual void setSource( NodeRef source ) override;

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

class Context {
  public:
	virtual ~Context();

	virtual ContextRef			createContext() = 0;
	virtual MixerNodeRef		createMixer() = 0;
	virtual OutputNodeRef		createOutput( DeviceRef device = Device::getDefaultOutput() ) = 0;
	virtual InputNodeRef		createInput( DeviceRef device = Device::getDefaultInput() ) = 0;

	static Context* instance();

	virtual void initialize();
	virtual void uninitialize();
	virtual void setRoot( RootNodeRef root )	{ mRoot = root; }

	//! If the root has not already been set, it is the default OutputNode
	virtual RootNodeRef getRoot();
	virtual void start();
	virtual void stop();


	bool isInitialized() const	{ return mInitialized; }

	// TODO: rename isEnabled() / setEnabled() ?
	bool isRunning() const		{ return mRunning; }

	//! convenience method to start / stop the graph via bool
	void setRunning( bool running = true );

  protected:
	Context() : mInitialized( false ), mRunning( false ) {}

	virtual void start( NodeRef node );
	virtual void stop( NodeRef node );


	RootNodeRef		mRoot;
	bool			mInitialized, mRunning;
};

} // namespace audio2