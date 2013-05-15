#pragma once

#include "audio2/Graph.h"
#include "audio2/RingBuffer.h"
#include "audio2/cocoa/Util.h"
#include <AudioUnit/AudioUnit.h>

namespace audio2 {

class DeviceAudioUnit;

struct RenderContext {
	Node *currentNode;
	BufferT buffer;
};

class AudioUnitNode {
  public:
	AudioUnitNode() : mAudioUnit( nullptr ), mRenderBus( 0 ), mShouldUseGraphRenderCallback( true )	{}
	virtual ~AudioUnitNode();
	virtual ::AudioUnit getAudioUnit() const	{ return mAudioUnit; }
	::AudioUnitScope getRenderBus() const	{ return mRenderBus; }

	bool shouldUseGraphRenderCallback() const	{ return mShouldUseGraphRenderCallback; }
  protected:
	::AudioUnit			mAudioUnit;
	::AudioUnitScope	mRenderBus;
	bool				mShouldUseGraphRenderCallback;
};

class OutputAudioUnit : public Output, public AudioUnitNode {
  public:
	OutputAudioUnit( DeviceRef device );
	virtual ~OutputAudioUnit() = default;

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	::AudioUnit getAudioUnit() const override;
	DeviceRef getDevice() override;

	size_t getBlockSize() const override;

  private:
	std::shared_ptr<DeviceAudioUnit> mDevice;
};

class InputAudioUnit : public Input, public AudioUnitNode {
  public:
	InputAudioUnit( DeviceRef device );
	virtual ~InputAudioUnit();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	::AudioUnit getAudioUnit() const override;
	DeviceRef getDevice() override;

	void render( BufferT *buffer ) override;

  private:
	static OSStatus inputCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList );

	std::shared_ptr<DeviceAudioUnit> mDevice;
	std::unique_ptr<RingBuffer> mRingBuffer;
	cocoa::AudioBufferListRef mBufferList;
};

class EffectAudioUnit : public Effect, public AudioUnitNode {
public:
	EffectAudioUnit( UInt32 subType );
	virtual ~EffectAudioUnit();

	void initialize() override;
	void uninitialize() override;

	// ???: is there a safer way to do this? Possibities:
	// - inherit from abstract AudioUnitNode (multiple-inheritance)
	// - Node owns a NodeImpl* pointer that can be dynamically casted to NodeImplAudioUnit
	// - These guys all inherit from AudioUnitNode - then Node needs a much larger interface
//		void* getNative() override	{ return mAudioUnit; }

	void setParameter( ::AudioUnitParameterID param, float val );

  private:
	UInt32		mEffectSubType;
};

class MixerAudioUnit : public Mixer, public AudioUnitNode {
public:
	MixerAudioUnit();
	virtual ~MixerAudioUnit();

	void initialize() override;
	void uninitialize() override;

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
};

class ConverterAudioUnit : public Node, public AudioUnitNode {
  public:
	ConverterAudioUnit( NodeRef source, NodeRef dest, size_t outputBlockSize );
	virtual ~ConverterAudioUnit();

	void initialize() override;
	void uninitialize() override;

  private:
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
	void connectRenderCallback( NodeRef node, RenderContext *context = nullptr, bool recursive = false );

	RenderContext mRenderContext;
};

} // namespace audio2