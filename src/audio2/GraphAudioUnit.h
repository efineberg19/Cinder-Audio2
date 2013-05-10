#pragma once

#include "audio2/Graph.h"

#include <AudioUnit/AudioUnit.h>

namespace audio2 {

	class DeviceAudioUnit;

	struct RenderContext {
		Node *currentNode;
		BufferT buffer;
	};

	class OutputAudioUnit : public Output {
	public:
		OutputAudioUnit( DeviceRef device );
		virtual ~OutputAudioUnit() = default;

		void initialize() override;
		void uninitialize() override;

		void start() override;
		void stop() override;

		DeviceRef getDevice() override;

		void* getNative() override;
		size_t getBlockSize() const override;

	private:
		static OSStatus renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList );

		std::shared_ptr<DeviceAudioUnit> mDevice;
	};

	class EffectAudioUnit : public Effect {
	public:
		EffectAudioUnit( UInt32 subType );
		virtual ~EffectAudioUnit();

		void initialize() override;
		void uninitialize() override;

		// ???: is there a safer way to do this? Possibities:
		// - inherit from abstract NodeAudioUnit (multiple-inheritance)
		// - Node owns a NodeImpl* pointer that can be dynamically casted to NodeImplAudioUnit
		// - These guys all inherit from NodeAudioUnit - then Node needs a much larger interface
		void* getNative() override	{ return mAudioUnit; }

		void setParameter( ::AudioUnitParameterID param, float val );

	private:
		UInt32		mEffectSubType;
		::AudioUnit	mAudioUnit;

	private:
		static OSStatus renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList );

		RenderContext mRenderContext;
	};

	class MixerAudioUnit : public Mixer {
	public:
		MixerAudioUnit();
		virtual ~MixerAudioUnit();

		void initialize() override;
		void uninitialize() override;
		void* getNative() override	{ return mAudioUnit; }

		size_t getNumBusses() override;
		void setNumBusses( size_t count ) override;
		bool isBusEnabled( size_t bus ) override;
		void setBusEnabled( size_t bus, bool enabled = true ) override;
		void setBusVolume( size_t bus, float volume ) override;
		float getBusVolume( size_t bus ) override;
		void setBusPan( size_t bus, float pan ) override;
		float getBusPan( size_t bus ) override;

	private:
		void checkBusIsValid( size_t bus );
		
		::AudioUnit	mAudioUnit;
	};

	class ConverterAudioUnit : public Node {
	public:
		ConverterAudioUnit( NodeRef source, NodeRef dest, size_t outputBlockSize );
		virtual ~ConverterAudioUnit();

		void initialize() override;
		void uninitialize() override;
		void* getNative() override	{ return mAudioUnit; }

	private:
		::AudioUnit	mAudioUnit;
		Node::Format mSourceFormat;
		RenderContext mRenderContext;

		friend class GraphAudioUnit;
	};

	class GraphAudioUnit : public Graph {
	public:
		virtual ~GraphAudioUnit();

		void initialize() override;
		void uninitialize() override;

	private:
		static OSStatus renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList );

		void initNode( NodeRef node );
		void uninitNode( NodeRef node );
		void connectRenderCallback( NodeRef node, RenderContext *context = nullptr );

		RenderContext mRenderContext;
	};

} // namespace audio2