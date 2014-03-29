/*
 Copyright (c) 2014, The Cinder Project

 This code is intended to be used with the Cinder C++ library, http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#define CI_ENABLE_XAUDIO2

#if ( _WIN32_WINNT < _WIN32_WINNT_VISTA ) || defined( CI_ENABLE_XAUDIO2 )
	#define CINDER_AUDIO_XAUDIO2

#include "cinder/audio2/Context.h"
#include "cinder/audio2/Buffer.h"
#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/msw/CinderXaudio.h" // TODO: fold this into this file, move XAPO stuff to test
#include "cinder/audio2/msw/MswUtil.h"

namespace cinder { namespace audio2 { namespace msw {

class NodeXAudioSourceVoice;
class NodeXAudio;

class NodeXAudio {
  public:
	NodeXAudio() {}
	virtual ~NodeXAudio();

  protected:
	std::vector<::XAUDIO2_EFFECT_DESCRIPTOR>	mEffectsDescriptors;
};

struct VoiceCallbackImpl;


class NodeXAudioSourceVoice : public Node, public NodeXAudio {
  public:
	virtual	~NodeXAudioSourceVoice();

	void start() override;
	void stop() override;

	//! Returns the native \a IXAudio2SourceVoice maintained by the \a Node.
	IXAudio2SourceVoice* getNative()	{ return mSourceVoice; }

	std::vector<::XAUDIO2_EFFECT_DESCRIPTOR>& getEffectsDescriptors() { return mEffectsDescriptors; }

  protected:
	void initialize() override;
	void uninitialize() override;

  private:
	NodeXAudioSourceVoice();

	void initSourceVoice();
	void uninitSourceVoice();

	void submitNextBuffer();

	::IXAudio2SourceVoice*						mSourceVoice;
	::XAUDIO2_BUFFER							mXAudioBuffer;
	std::vector<::XAUDIO2_EFFECT_DESCRIPTOR>	mEffectsDescriptors;
	BufferInterleaved							mBufferInterleaved;
	std::unique_ptr<VoiceCallbackImpl>			mVoiceCallback;

	friend class ContextXAudio;
	friend class NodeEffectXAudioXapo;
};


struct EngineCallbackImpl;

class LineOutXAudio : public LineOut, public NodeXAudio {
  public:
	LineOutXAudio( DeviceRef device, const Format &format = Format() );
	virtual ~LineOutXAudio();

	void start() override;
	void stop() override;

	//! Returns the IXAudio2SourceVoice used to submit audio buffers to the mastering voice
	::IXAudio2SourceVoice*	getSourceVoice() const	{ return mSourceVoice; }
	//! Sets whether filter usage is enabled within the audio context (default = true). Disabling may increase performance.
	//! \note must be set before this LineOutXAudio is initialized to have any effect.
	void setFilterEffectsEnabled( bool b = true )	{ mFilterEnabled = b; }
	//! Returns whether filter usage is enabled within this audio context (default = true). \see NodeEffectXAudioFilter
	bool isFilterEffectsEnabled() const				{ return mFilterEnabled; }

  protected:
	void initialize() override;
	void uninitialize() override;

  private:

	void initMasterVoice();
	void initSourceVoice();
	void submitNextBuffer();

	::IXAudio2SourceVoice*					mSourceVoice;
	::XAUDIO2_BUFFER						mXAudioBuffer;
	std::unique_ptr<VoiceCallbackImpl>		mVoiceCallback;
	BufferInterleaved						mBufferInterleaved;
	bool									mFilterEnabled;

	friend struct VoiceCallbackImpl;
};

class NodeEffectXAudioXapo : public NodeEffect, public NodeXAudio {
  public:
	//! These enum names match the class uuid names in xapofx.h. TODO: consider just passing in the REFCLSID
	enum XapoType { FXEcho, FXEQ, FXMasteringLimiter, FXReverb };

	NodeEffectXAudioXapo( XapoType type, const Format &format = Format() );
	virtual ~NodeEffectXAudioXapo();

	// TODO: get/set params should throw if a bad HRESULT shows up because of this

	template<typename ParamsT>
	void getParams( ParamsT *params )			{ getParams( static_cast<void *>( params ), sizeof( *params ) ); }

	template<typename ParamsT>
	void setParams( const ParamsT &params )		{ setParams( static_cast<const void *>( &params ), sizeof( params ) ); }

  protected:
	void initialize() override;

  private:
	void getParams( void *params, size_t sizeParams );
	void setParams( const void *params, size_t sizeParams );

	void makeXapo( REFCLSID clsid );
	void notifyConnected();

	std::unique_ptr<::IUnknown, msw::ComReleaser>	mXapo;
	XapoType										mType;
	::XAUDIO2_EFFECT_DESCRIPTOR						mEffectDesc;
	size_t											mChainIndex;

	friend class ContextXAudio;
};

//! \note Due to XAudio2 limitations, only one EffectXAudioFilter can be attached in series. Connecting another will simply overwrite the first and you will only hear the second filter.
class NodeEffectXAudioFilter : public NodeEffect, public NodeXAudio {
  public:
	NodeEffectXAudioFilter( const Format &format = Format() );
	virtual ~NodeEffectXAudioFilter();

	void getParams( ::XAUDIO2_FILTER_PARAMETERS *params );
	void setParams( const ::XAUDIO2_FILTER_PARAMETERS &params );

  protected:
	void initialize() override;
	void uninitialize() override;
};

class ContextXAudio : public Context {
  public:
	ContextXAudio();
	virtual ~ContextXAudio();

	LineOutRef	createLineOut( const DeviceRef &device, const Node::Format &format = Node::Format() ) override;
	//! No LineIn is available via XAudio2 path, returns an empty \a LineInRef
	LineInRef	createLineIn( const DeviceRef &device, const Node::Format &format = Node::Format()  ) override;

	//! Overridden to also start XAudio2 engine.
	void start() override;
	//! Overridden to also stop XAudio2 engine.
	void stop() override;

	//! Returns the LineOutXAudio that is used for this Context's NodeOutput.
	std::shared_ptr<LineOutXAudio>	getLineOutXAudio() const;
	//! Returns a pointer to the \a IXAudio2 instance owned by this context
	::IXAudio2*						getXAudio() const	{ return mXAudio; }
	//! Returns a pointer to the \a IXAudio2 instance owned by this context
	::IXAudio2MasteringVoice*		getMasteringVoice() const	{ return mMasteringVoice; }

  private:
	//! Overridden to disable setting the NodeOutput, only the default is supported, which is automatically created.
	void setOutput( const NodeOutputRef &output ) override;

	void initXAudio2();
	void initMasteringVoice();

	::IXAudio2*							mXAudio;
	::IXAudio2MasteringVoice*			mMasteringVoice;
	std::unique_ptr<EngineCallbackImpl> mEngineCallback;
};

class DeviceManagerXAudio : public DeviceManager {
public:
	DeviceRef getDefaultOutput() override;
	DeviceRef getDefaultInput() override;

	const std::vector<DeviceRef>& getDevices() override;

	std::string getName( const DeviceRef &device ) override;
	size_t getNumInputChannels( const DeviceRef &device ) override;
	size_t getNumOutputChannels( const DeviceRef &device ) override;
	size_t getSampleRate( const DeviceRef &device ) override;
	size_t getFramesPerBlock( const DeviceRef &device ) override;

	void setSampleRate( const DeviceRef &device, size_t sampleRate ) override;
	void setFramesPerBlock( const DeviceRef &device, size_t framesPerBlock ) override;

private:
	const DeviceRef&	getDefaultDevice();
		
	DeviceRef	mDefaultDevice;
};

} } } // namespace cinder::audio2::msw

#endif //  CINDER_AUDIO_XAUDIO2
