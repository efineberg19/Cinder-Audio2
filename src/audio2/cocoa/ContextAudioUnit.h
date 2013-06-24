#pragma once

#include "audio2/Context.h"
#include "audio2/GeneratorNode.h"
#include "audio2/EffectNode.h"
#include "audio2/RingBuffer.h"
#include "audio2/cocoa/CinderCoreAudio.h"

#include <AudioUnit/AudioUnit.h>

namespace audio2 { namespace cocoa {

class DeviceAudioUnit;

struct RenderCallbackContext {
	Node *currentNode;
	Buffer buffer;
};

class NodeAudioUnit {
  public:
	NodeAudioUnit() : mAudioUnit( nullptr ), mRenderBus( 0 ), mShouldUseGraphRenderCallback( true )	{}
	virtual ~NodeAudioUnit();
	virtual ::AudioUnit getAudioUnit() const	{ return mAudioUnit; }
	::AudioUnitScope getRenderBus() const	{ return mRenderBus; }

	bool shouldUseGraphRenderCallback() const	{ return mShouldUseGraphRenderCallback; }
  protected:
	::AudioUnit			mAudioUnit;
	::AudioUnitScope	mRenderBus;
	bool				mShouldUseGraphRenderCallback;
};

class OutputAudioUnit : public OutputNode, public NodeAudioUnit {
  public:
	OutputAudioUnit( DeviceRef device, const Format &format = Format() );
	virtual ~OutputAudioUnit() = default;

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	::AudioUnit getAudioUnit() const override;
	DeviceRef getDevice() override;

  private:
	std::shared_ptr<DeviceAudioUnit> mDevice;
};

class InputAudioUnit : public InputNode, public NodeAudioUnit {
  public:
	InputAudioUnit( DeviceRef device, const Format &format = Format() );
	virtual ~InputAudioUnit();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	::AudioUnit getAudioUnit() const override;
	DeviceRef getDevice() override;

	void process( Buffer *buffer ) override;

  private:
	static OSStatus inputCallback( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList );

	std::shared_ptr<DeviceAudioUnit> mDevice;
	std::unique_ptr<RingBuffer> mRingBuffer;
	cocoa::AudioBufferListRef mBufferList;
};

// TODO: when stopped / mEnabled = false; kAudioUnitProperty_BypassEffect should be used
class EffectAudioUnit : public EffectNode, public NodeAudioUnit {
  public:
	EffectAudioUnit( UInt32 subType, const Format &format = Format() );
	virtual ~EffectAudioUnit();

	void initialize() override;
	void uninitialize() override;

	void setParameter( ::AudioUnitParameterID param, float val );

  private:
	UInt32		mEffectSubType;
};

class MixerAudioUnit : public MixerNode, public NodeAudioUnit {
  public:
	MixerAudioUnit( const Format &format = Format() );
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

class ConverterAudioUnit : public Node, public NodeAudioUnit {
  public:
	ConverterAudioUnit( NodeRef source, NodeRef dest );
	virtual ~ConverterAudioUnit();

	void initialize() override;
	void uninitialize() override;

  private:
	size_t mSourceNumChannels;
	RenderCallbackContext mRenderContext;

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

	void connectRenderCallback( NodeRef node, RenderCallbackContext *context, ::AURenderCallback callback, bool recursive );

	static OSStatus renderCallback( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList );
	static OSStatus renderCallbackRoot( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList );
	static OSStatus renderCallbackConverter( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList );

	// TODO: consider making these abstract methods in Context
	void initNode( NodeRef node );
	void uninitNode( NodeRef node );

	
	RenderCallbackContext mRenderContext;
};

} } // namespace audio2::cocoa