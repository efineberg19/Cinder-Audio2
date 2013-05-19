#pragma once

#include "audio2/Graph.h"
#include "audio2/msw/xaudio.h"

namespace audio2 {

// ??? useful on win?
//struct RenderContext {
//	Node *currentNode;
//	BufferT buffer;
//};

class XAudioNode {
  public:
	XAudioNode() {}
	virtual ~XAudioNode();

	void setXAudio( ::IXAudio2 *xaudio )	{ mXaudio = xaudio; }
  protected:
	  ::IXAudio2 *mXaudio;
};

class DeviceOutputXAudio;

class OutputXAudio : public Output, public XAudioNode {
  public:
	OutputXAudio( DeviceRef device );
	virtual ~OutputXAudio() {}

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	DeviceRef getDevice() override;

	size_t getBlockSize() const override;

  private:
	std::shared_ptr<DeviceOutputXAudio> mDevice;
};

struct VoiceCallbackImpl;

class SourceXAudio : public Producer, public XAudioNode {
  public:
	SourceXAudio();
	~SourceXAudio();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

  private:
	  void submitNextBuffer();

	  ::IXAudio2SourceVoice						*mSourceVoice;
	  std::vector<::XAUDIO2_EFFECT_DESCRIPTOR>	mEffectsDescriptors;
	  BufferT									mBuffer;
	  std::unique_ptr<VoiceCallbackImpl>		mVoiceCallback;
};

//class InputXAudio : public Input, public XAudioNode {
//  public:
//	InputAudioUnit( DeviceRef device );
//	virtual ~InputAudioUnit();
//
//	void initialize() override;
//	void uninitialize() override;
//
//	void start() override;
//	void stop() override;
//
//	::AudioUnit getAudioUnit() const override;
//	DeviceRef getDevice() override;
//
//	void render( BufferT *buffer ) override;
//
//  private:
//	static OSStatus inputCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList );
//
//	std::shared_ptr<DeviceAudioUnit> mDevice;
//	std::unique_ptr<RingBuffer> mRingBuffer;
//	cocoa::AudioBufferListRef mBufferList;
//};

//class EffectXAudio : public Effect, public XAudioNode {
//public:
//	EffectXAudio();
//	virtual ~EffectXAudio();
//
//	void initialize() override;
//	void uninitialize() override;
//
//	//void setParameter( ::AudioUnitParameterID param, float val );
//
//  private:
//};

//class MixerXAudio : public Mixer, public XAudioNode {
//public:
//	MixerXAudio();
//	virtual ~MixerXAudio();
//
//	void initialize() override;
//	void uninitialize() override;
//
//	size_t getNumBusses() override;
//	void setNumBusses( size_t count ) override;
//	bool isBusEnabled( size_t bus ) override;
//	void setBusEnabled( size_t bus, bool enabled = true ) override;
//	void setBusVolume( size_t bus, float volume ) override;
//	float getBusVolume( size_t bus ) override;
//	void setBusPan( size_t bus, float pan ) override;
//	float getBusPan( size_t bus ) override;
//
//  private:
//	void checkBusIsValid( size_t bus );
//};

//class ConverterXAudio : public Node, public XAudioNode {
//  public:
//	ConverterXAudio( NodeRef source, NodeRef dest, size_t outputBlockSize );
//	virtual ~ConverterXAudio();
//
//	void initialize() override;
//	void uninitialize() override;
//
//  private:
//	Node::Format mSourceFormat;
//	RenderContext mRenderContext;
//
//};

class GraphXAudio : public Graph {
  public:
	virtual ~GraphXAudio();

	void initialize() override;
	void uninitialize() override;

  private:

	void initNode( NodeRef node );
	void uninitNode( NodeRef node );
	//void connectRenderCallback( NodeRef node, RenderContext *context = nullptr, bool recursive = false );

	//RenderContext mRenderContext;
};

} // namespace audio2