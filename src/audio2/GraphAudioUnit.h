#pragma once

#include "audio2/Graph.h"

#include <AudioUnit/AudioUnit.h>

namespace audio2 {

	class DeviceAudioUnit;

	struct RenderContext {
		Node *node;
		BufferT *buffer;
	};

	class OutputAudioUnit : public Output {
	public:
		OutputAudioUnit( DeviceRef device );
		virtual ~OutputAudioUnit() = default;

		void initialize() override;
		void uninitialize() override;

		void start() override;
		void stop() override;

		BufferT& getInternalBuffer() { return mBuffer; } // TEMP

	private:
		static OSStatus renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList );
		void renderNode( NodeRef node, BufferT *buffer, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *auBufferList );

		std::shared_ptr<DeviceAudioUnit> mDevice;
		::AudioStreamBasicDescription mASBD; // TODO: no reason to keep this around that I can think of
		BufferT mBuffer;

		RenderContext mRenderContext;
	};

	class ProcessorAudioUnit : public Processor {
	public:
		ProcessorAudioUnit( UInt32 effectSubType );
		virtual ~ProcessorAudioUnit() = default;

		void initialize() override;

		// ???: is there a safer way to do this? Can I protect against someone making their own Processor subclass, overriding getNative and returning something other than type AudioUnit?
		void *getNative() override	{ return mAudioUnit; }

		void setParameter( ::AudioUnitParameterID param, float val );

	protected:
		UInt32		mEffectSubType;
		::AudioUnit	mAudioUnit;

	private:
		static OSStatus renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList );

		RenderContext mRenderContext;
	};

	class GraphAudioUnit : public Graph {

	};

} // namespace audio2