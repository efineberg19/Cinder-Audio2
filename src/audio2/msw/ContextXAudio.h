#pragma once

#include "audio2/Context.h"
#include "audio2/Buffer.h"
#include "audio2/EffectNode.h"
#include "audio2/msw/xaudio.h"
#include "audio2/msw/util.h"

namespace audio2 { namespace msw {

class SourceVoiceXAudio;
class NodeXAudio;

struct XAudioVoice {
	XAudioVoice() : voice( nullptr ), node( nullptr )	{}
	XAudioVoice( ::IXAudio2Voice *voice, NodeXAudio *parent ) : voice( voice ), node( parent ) {}
	::IXAudio2Voice *voice;
	NodeXAudio *node;
};

class NodeXAudio {
  public:
	NodeXAudio() : mFilterEnabled( false ), mFilterConnected( false ) {}
	virtual ~NodeXAudio();

	void setXAudio( ::IXAudio2 *xaudio )		{ mXAudio = xaudio; }

	// Subclasses override these methods to return their xaudio voice if they have one,
	// otherwise the default implementation recurses through sources to find the goods.
	// Node must be passed in here to traverse it's children and I want to avoid the complexities of dual inheriting from Node.
	// (this is a +1 for using a pimpl approach instead of dual inheritance)
	virtual XAudioVoice getXAudioVoice( NodeRef node );

	std::vector<::XAUDIO2_EFFECT_DESCRIPTOR>& getEffectsDescriptors() { return mEffectsDescriptors; }

	void setFilterEnabled( bool b = true )	{ mFilterEnabled = b; }
	bool isFilterEnabled() const			{ return mFilterEnabled; }
	void setFilterConnected( bool b = true )	{ mFilterConnected = b; }
	bool isFilterConnected() const			{ return mFilterConnected; }

  protected:
	::IXAudio2 *mXAudio;
	std::vector<::XAUDIO2_EFFECT_DESCRIPTOR> mEffectsDescriptors;

	bool								mFilterEnabled, mFilterConnected;
};

class DeviceOutputXAudio;

class OutputXAudio : public OutputNode, public NodeXAudio {
  public:
	OutputXAudio( DeviceRef device );
	virtual ~OutputXAudio() {}

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	DeviceRef getDevice() override;

	size_t getBlockSize() const override;

	bool supportsSourceFormat( const Format &sourceFormat ) const override;

  private:
	std::shared_ptr<DeviceOutputXAudio> mDevice;
};

struct VoiceCallbackImpl;

class SourceVoiceXAudio : public Node, public NodeXAudio {
  public:
	SourceVoiceXAudio();
	~SourceVoiceXAudio();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	XAudioVoice		getXAudioVoice( NodeRef node ) override			{ return XAudioVoice( static_cast<::IXAudio2Voice *>( mSourceVoice ), this ); }
  
	bool isRunning() const	{ return mIsRunning; }

  private:
	void submitNextBuffer();
	void renderNode( NodeRef node );

	::IXAudio2SourceVoice						*mSourceVoice;
	::XAUDIO2_BUFFER							mXAudio2Buffer;
	std::vector<::XAUDIO2_EFFECT_DESCRIPTOR>	mEffectsDescriptors;
	Buffer										mBuffer, mBufferInterleaved;
	std::unique_ptr<VoiceCallbackImpl>			mVoiceCallback;
	bool										mIsRunning;
};

class EffectXAudioXapo : public EffectNode, public NodeXAudio {
public:
	enum XapoType { FXEcho, FXEQ, FXMasteringLimiter, FXReverb };

	EffectXAudioXapo( XapoType type );
	virtual ~EffectXAudioXapo();

	void initialize() override;
	void uninitialize() override;

	// TODO: get/set params should throw if a bad HRESULT shows up because of this

	template<typename ParamsT>
	void getParams( ParamsT *params )			{ getParams( static_cast<void *>( params ), sizeof( *params ) ); }

	// TODO: ask AFB if setter should be *params or &params
	template<typename ParamsT>
	void setParams( const ParamsT &params )		{ setParams( static_cast<const void *>( &params ), sizeof( params ) ); }
 
private:

	void getParams( void *params, size_t sizeParams );
	void setParams( const void *params, size_t sizeParams );

	void makeXapo( REFCLSID clsid );
	std::unique_ptr<::IUnknown, msw::ComReleaser> mXapo;
	XapoType mType;
	size_t mChainIndex;
};

class EffectXAudioFilter : public EffectNode, public NodeXAudio {
public:

	EffectXAudioFilter();
	virtual ~EffectXAudioFilter();

	void initialize() override;
	void uninitialize() override;

	void getParams( ::XAUDIO2_FILTER_PARAMETERS *params );
	void setParams( const ::XAUDIO2_FILTER_PARAMETERS &params ); // TODO: ask AFB if this shoul be * of &

private:

};


class MixerXAudio : public MixerNode, public NodeXAudio {
public:
	MixerXAudio();
	virtual ~MixerXAudio();

	void initialize() override;
	void uninitialize() override;

	size_t getNumBusses() override;
	void setNumBusses( size_t count ) override;
	void setMaxNumBusses( size_t count ) override;
	bool isBusEnabled( size_t bus ) override;
	void setBusEnabled( size_t bus, bool enabled = true ) override;
	void setBusVolume( size_t bus, float volume ) override;
	float getBusVolume( size_t bus ) override;
	void setBusPan( size_t bus, float pan ) override;
	float getBusPan( size_t bus ) override;

	XAudioVoice		getXAudioVoice( NodeRef node ) override			{ return XAudioVoice( static_cast<::IXAudio2Voice *>( mSubmixVoice ), this ); }

	bool supportsSourceFormat( const Node::Format &sourceFormat ) const override;

  private:
	void checkBusIsValid( size_t bus );

	::IXAudio2SubmixVoice *mSubmixVoice;
};

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

class ContextXAudio : public Context {
  public:
	virtual ~ContextXAudio();

	virtual ContextRef		createContext() override	{ return ContextRef( new ContextXAudio() ); }
	virtual MixerNodeRef	createMixer() override	{ return MixerNodeRef( new MixerXAudio() ); }
	virtual OutputNodeRef	createOutput( DeviceRef device ) override	{ return OutputNodeRef( new OutputXAudio( device ) ); }

	//! If deployment target is 0x601 (win xp) or greater, uses InputWasapi
	virtual InputNodeRef	createInput( DeviceRef device ) override;

	void initialize() override;
	void uninitialize() override;

  private:

	void initNode( NodeRef node );
	void uninitNode( NodeRef node );
	void setXAudio( NodeRef node );
	void initEffects( NodeRef node );
};

} } // namespace audio2::msw