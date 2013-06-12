#pragma once

#include "audio2/Context.h"
#include "audio2/GeneratorNode.h"
#include "audio2/EffectNode.h"
#include "audio2/RingBuffer.h"
#include "audio2/cocoa/Util.h"

#include <AudioUnit/AudioUnit.h>

namespace audio2 { namespace cocoa {

class DeviceAudioUnit;

struct RenderContext {
	Node *currentNode;
	Buffer buffer;
};

	// TODO: rename to NodeAudioUnit for consistency
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

class OutputAudioUnit : public OutputNode, public AudioUnitNode {
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

class InputAudioUnit : public InputNode, public AudioUnitNode {
  public:
	InputAudioUnit( DeviceRef device );
	virtual ~InputAudioUnit();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	::AudioUnit getAudioUnit() const override;
	DeviceRef getDevice() override;

	void process( Buffer *buffer ) override;

  private:
	static OSStatus inputCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList );

	std::shared_ptr<DeviceAudioUnit> mDevice;
	std::unique_ptr<RingBuffer> mRingBuffer;
	cocoa::AudioBufferListRef mBufferList;
};

// TODO: when stopped / mEnabled = false; kAudioUnitProperty_BypassEffect should be used
class EffectAudioUnit : public EffectNode, public AudioUnitNode {
  public:
	EffectAudioUnit( UInt32 subType );
	virtual ~EffectAudioUnit();

	void initialize() override;
	void uninitialize() override;

	void setParameter( ::AudioUnitParameterID param, float val );

  private:
	UInt32		mEffectSubType;
};

class MixerAudioUnit : public MixerNode, public AudioUnitNode {
  public:
	MixerAudioUnit();
	virtual ~MixerAudioUnit();

	void initialize() override;
	void uninitialize() override;

	size_t getNumBusses() override;
	void setNumBusses( size_t count ) override;
	void setMaxNumBusses( size_t count );

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

	friend class ContextAudioUnit;
};

class ContextAudioUnit : public Context {
  public:
	virtual ~ContextAudioUnit();

	virtual ContextRef			createContext() override					{ return ContextRef( new ContextAudioUnit() ); }
	virtual OutputNodeRef		createOutput( DeviceRef device ) override	{ return OutputNodeRef( new OutputAudioUnit( device ) ); }
	virtual InputNodeRef		createInput( DeviceRef device ) override	{ return InputNodeRef( new InputAudioUnit( device ) ); }
	virtual MixerNodeRef		createMixer() override						{ return MixerNodeRef( new MixerAudioUnit() ); }

	void initialize() override;
	void uninitialize() override;

  private:
	static OSStatus renderCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList );

	void initNode( NodeRef node );
	void uninitNode( NodeRef node );
	void connectRenderCallback( NodeRef node, RenderContext *context = nullptr, bool recursive = false );

	RenderContext mRenderContext;
};

} } // namespace audio2::cocoa