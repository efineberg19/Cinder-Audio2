#pragma once

#include "audio2/Device.h"

#include <memory>
#include <vector>

namespace audio2 {

	typedef std::shared_ptr<class Graph> GraphRef;
	typedef std::shared_ptr<class Node> NodeRef;
	typedef std::weak_ptr<class Node> NodeWeakRef;

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

		const std::vector<NodeRef>& getSources()	{ return mSources; }
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
	};

	// TODO: rename Effect
	class Processor : public Node {
	public:
		Processor() : Node() {}
		virtual ~Processor() = default;

		virtual void connect( NodeRef source );
	};


	class Output : public Consumer {
	public:
//		static OutputRef create( DeviceRef device = Device::getDefaultOutput() );

		// ???: device param here necessary?
		Output( DeviceRef device ) : Consumer() {}
		virtual ~Output() {}

	protected:
	};

	class Graph {
	public:
		virtual ~Graph();

		virtual void initialize();
		virtual void uninitialize();
		virtual void setOutput( ConsumerRef output )	{ mOutput = output; }
		virtual void start();
		virtual void stop();

		bool isRunning() const	{ return mRunning; }

	protected:
		Graph() : mInitialized( false ), mRunning( false ) {}

		ConsumerRef	mOutput;
		bool		mInitialized, mRunning;
	};

} // namespace audio2