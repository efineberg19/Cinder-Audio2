#pragma once

#include "audio2/Graph.h"

#include <AudioUnit/AudioUnit.h>

// TODO: dispose units in uninitialize

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
		::AudioStreamBasicDescription mASBD; // TODO: no reason to keep this around that I can think of
	};

	class EffectAudioUnit : public Effect {
	public:
		EffectAudioUnit( UInt32 subType );
		virtual ~EffectAudioUnit() = default;

		void initialize() override;

		// ???: is there a safer way to do this? Can I protect against someone making their own Effect subclass, overriding getNative and returning something other than type AudioUnit?
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
		virtual ~MixerAudioUnit() = default;

		void initialize() override;
		void* getNative() override	{ return mAudioUnit; }
	private:
		::AudioUnit	mAudioUnit;
	};

	class GraphAudioUnit : public Graph {
	public:

		void initialize() override;
		void uninitialize() override;

	private:
		static OSStatus renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList );

		void initializeNode( NodeRef node );

		RenderContext mRenderContext;
	};

} // namespace audio2