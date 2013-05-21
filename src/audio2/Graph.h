#pragma once

#include "audio2/Device.h"

#include <memory>
#include <vector>

namespace audio2 {

typedef std::shared_ptr<class Graph> GraphRef;
typedef std::shared_ptr<class Node> NodeRef;
typedef std::weak_ptr<class Node> NodeWeakRef;

typedef std::shared_ptr<class Mixer> MixerRef;
typedef std::shared_ptr<class Consumer> ConsumerRef;
typedef std::shared_ptr<class Producer> ProducerRef;
typedef std::shared_ptr<class BufferTap> BufferTapRef;

typedef std::vector<float>		ChannelT;
typedef std::vector<ChannelT>	BufferT;

class Node : public std::enable_shared_from_this<Node> {
  public:
	virtual ~Node();

	struct Format {
		Format() : mSampleRate( 0 ), mNumChannels( 0 ), mWantsDefaultFormatFromParent( false )
		{}

		virtual bool isComplete() const	{ return ( mSampleRate && mNumChannels ); }

		size_t	getSampleRate() const	{ return mSampleRate; }
		void	setSampleRate( size_t sampleRate )	{ mSampleRate = sampleRate; }
		size_t	getNumChannels() const	{ return mNumChannels; }
		void	setNumChannels( size_t numChannels )	{ mNumChannels = numChannels; }
		bool	wantsDefaultFormatFromParent() const	{ return mWantsDefaultFormatFromParent; }
		void	setWantsDefaultFormatFromParent( bool b = true )	{ mWantsDefaultFormatFromParent = b; }

  private:
		size_t mSampleRate, mNumChannels;
		bool mWantsDefaultFormatFromParent;
	};

	virtual void initialize()	{}
	virtual void uninitialize()	{}
	virtual void start()		{}
	virtual void stop()			{}

	// ???: does making BufferT const help make it less expandable? Because it shouldb't be resize()'ed
	virtual void render( BufferT *buffer )	{}

	std::vector<NodeRef>& getSources()			{ return mSources; }
	NodeRef getParent()							{ return mParent.lock(); }
	void setParent( NodeRef parent )			{ mParent = parent; }

	Format& getFormat()	{ return mFormat; }

	//! Default implementation returns the format for the first source
	virtual const Format& getSourceFormat();

	const std::string& getTag()	const	{ return mTag; }

  protected:
	Node() : mInitialized( false )	{}

	std::vector<NodeRef>	mSources;
	NodeWeakRef				mParent;
	Format					mFormat;
	bool					mInitialized;
	std::string				mTag;

  private:
	  Node( Node const& );
	  Node& operator=( Node const& );
};

// TODO: these names don't really make sense here, it's just a feable attempt at avoiding naiming these Input / Output or Generator / ??

class Producer : public Node {
  public:
	Producer() : Node() {}
	virtual ~Producer() {}
};

class Input : public Producer {
  public:
	Input( DeviceRef device ) : Producer() {}
	virtual ~Input() {}

	virtual DeviceRef getDevice() = 0;
};

class Consumer : public Node {
  public:
	Consumer() : Node() {}
	virtual ~Consumer() {}

	virtual void connect( NodeRef source );
	virtual size_t getBlockSize() const = 0;
};

class Output : public Consumer {
  public:

	// ???: device param here necessary?
	Output( DeviceRef device ) : Consumer() {}
	virtual ~Output() {}

	virtual DeviceRef getDevice() = 0;

  protected:
};

class Effect : public Node {
  public:
	Effect() : Node() {}
	virtual ~Effect() {}

	virtual void connect( NodeRef source );
};

class RingBuffer;

class BufferTap : public Node {
  public:
	BufferTap( size_t bufferSize = 1024 );
	virtual ~BufferTap();

	const ChannelT& getChannel( size_t channel = 0 );
	const BufferT& getBuffer();

	virtual void initialize() override;
	virtual void render( BufferT *buffer ) override;

	virtual void connect( NodeRef source );

  private:
	std::vector<std::unique_ptr<RingBuffer> > mRingBuffers;
	BufferT mCopiedBuffer;
	size_t mBufferSize;
};

class Mixer : public Node {
  public:
	Mixer() : Node(), mMaxNumBusses( 20 ) {}
	virtual ~Mixer() {}

	virtual void connect( NodeRef source );
	virtual void connect( NodeRef source, size_t bus );

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
	virtual void setOutput( ConsumerRef output )	{ mOutput = output; }
	virtual ConsumerRef getOutput() const	{ return mOutput; }
	virtual void start();
	virtual void stop();

	bool isRunning() const	{ return mRunning; }

  protected:
	Graph() : mInitialized( false ), mRunning( false ) {}

	virtual void start( NodeRef node );
	virtual void stop( NodeRef node );


	ConsumerRef	mOutput;
	bool		mInitialized, mRunning;
};

} // namespace audio2