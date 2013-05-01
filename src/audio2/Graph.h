#pragma once

#include "audio2/Device.h"

#include <memory>
#include <vector>

namespace audio2 {

	typedef std::shared_ptr<class Node> NodeRef;
	typedef std::weak_ptr<class Node> NodeWeakRef;

	typedef std::shared_ptr<class Output> OutputRef;

	class Node : public std::enable_shared_from_this<Node> {
	public:

		struct Format {
			Format() : mSampleRate( 0 ), mNumChannels( 0 ), mWantsDefaultFormatFromParent( false )
			{}

			virtual bool isComplete() const	{ return ( mSampleRate && mNumChannels ); }

			size_t mSampleRate, mNumChannels;
			bool mWantsDefaultFormatFromParent;
		};

		virtual void initialize()	{}
		virtual void uninitialize()	{}

	protected:
		Node() : mInitialized( false )	{}
		Node( Node const& )				= delete;
		Node& operator=( Node const& )	= delete;
		virtual ~Node()					= default;

		std::vector<NodeRef>	mSources;
		NodeWeakRef				mParent;
		Format					mFormat;
		bool					mInitialized;
	};

	class Output : public Node {
	public:
		Output() : Node() {}
		virtual ~Output() = default;

		virtual void start() = 0;
		virtual void stop() = 0;
	};

	class SpeakerOutput : public Output {
	public:
//		static OutputRef create( DeviceRef device = Device::getDefaultOutput() );

		// ???: device param here necessary?
		SpeakerOutput( DeviceRef device ) : Output() {}
		virtual ~SpeakerOutput() {}

	protected:
	};

	class Graph {

	};

} // namespace audio2