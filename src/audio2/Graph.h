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

	//! vector of channels
	typedef std::vector<std::vector<float> > BufferT;

	class Node : public std::enable_shared_from_this<Node> {
	public:

		struct Format {
			Format() : mSampleRate( 0 ), mNumChannels( 0 ), mWantsDefaultFormatFromParent( false ), mIsNative( false )
			{}

			virtual bool isComplete() const	{ return ( mSampleRate && mNumChannels ); }

			bool	isNative() const	{ return mIsNative; }
			size_t	getSampleRate() const	{ return mSampleRate; }
			size_t	getNumChannels() const	{ return mNumChannels; }

			size_t mSampleRate, mNumChannels;
			bool mWantsDefaultFormatFromParent, mIsNative;
		};

		virtual void initialize()	{}
		virtual void uninitialize()	{}

		// ???: does making BufferT const help make it less expandable? Because it shouldb't be resize()'ed
		virtual void render( BufferT *buffer );

		virtual void* getNative()	{ return NULL; }

		std::vector<NodeRef>& getSources()			{ return mSources; }
		NodeRef getParent()							{ return mParent.lock(); }
		void setParent( NodeRef parent )			{ mParent = parent; }

		Format& getFormat()	{ return mFormat; }
		
	protected:
		Node() : mInitialized( false )	{}
		Node( Node const& )				= delete;
		Node& operator=( Node const& )	= delete;
		virtual ~Node()					= default;

		std::vector<NodeRef>	mSources;
		NodeWeakRef				mParent;
		Format					mFormat;
		bool					mInitialized;
		std::string				mTag;
	};

	// TODO: these names don't really make sense here, it's just a feable attempt at avoiding naiming these Input / Output or Generator / ??
	
	class Producer : public Node {
	public:
		Producer() : Node() {}
		virtual ~Producer() = default;

//		virtual void start() = 0;
//		virtual void stop() = 0;
	};

	class Consumer : public Node {
	public:
		Consumer() : Node() {}
		virtual ~Consumer() = default;

		virtual void start() = 0;
		virtual void stop() = 0;

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
		virtual ~Effect() = default;

		virtual void connect( NodeRef source );
	};
	
	class Mixer : public Node {
	public:
		Mixer() : Node() {}
		virtual ~Mixer() = default;

		virtual void connect( NodeRef source );
		virtual void connect( NodeRef source, size_t bus );

		virtual size_t getNumBusses() = 0;
		virtual void setNumBusses( size_t count ) = 0;
		virtual bool isBusEnabled( size_t bus ) = 0;
		virtual void setBusEnabled( size_t bus, bool enabled = true ) = 0;
		virtual void setBusVolume( size_t bus, float volume ) = 0;
		virtual float getBusVolume( size_t bus ) = 0;
		virtual void setBusPan( size_t bus, float pan ) = 0;
		virtual float getBusPan( size_t bus ) = 0;
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

		ConsumerRef	mOutput;
		bool		mInitialized, mRunning;
	};

} // namespace audio2