/*
 Copyright (c) 2013, The Cinder Project

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

#include "audio2/Context.h"
#include "audio2/Buffer.h"
#include "audio2/EffectNode.h"
#include "audio2/msw/xaudio.h"
#include "audio2/msw/util.h"

// TODO: all IXAudio2Voice's should only be Destroy()'d when their containing object
//       is destroyed - not uninitialize. Use unique_ptr's for this

namespace cinder { namespace audio2 { namespace msw {

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

	//void setXAudio( ::IXAudio2 *xaudio )		{ mXAudio = xaudio; }

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
	//::IXAudio2 *mXAudio;
	std::vector<::XAUDIO2_EFFECT_DESCRIPTOR> mEffectsDescriptors;

	bool								mFilterEnabled, mFilterConnected;
};

class DeviceOutputXAudio;

class LineOutXAudio : public LineOutNode, public NodeXAudio {
  public:
	LineOutXAudio( DeviceRef device, const Format &format = Format() );
	virtual ~LineOutXAudio() {}

	std::string virtual getTag()				{ return "LineOutXAudio"; }

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	DeviceRef getDevice() override;

	bool supportsSourceNumChannels( size_t numChannels ) const override;

  private:
	std::shared_ptr<DeviceOutputXAudio> mDevice;
};

struct VoiceCallbackImpl;

class SourceVoiceXAudio : public Node, public NodeXAudio {
  public:
	virtual	~SourceVoiceXAudio();

	std::string virtual getTag()				{ return "SourceVoiceXAudio"; }

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	XAudioVoice		getXAudioVoice( NodeRef node ) override			{ return XAudioVoice( static_cast<::IXAudio2Voice *>( mSourceVoice ), this ); }

  private:
	SourceVoiceXAudio();

	void submitNextBuffer();

	::IXAudio2SourceVoice						*mSourceVoice;
	::XAUDIO2_BUFFER							mXAudio2Buffer;
	std::vector<::XAUDIO2_EFFECT_DESCRIPTOR>	mEffectsDescriptors;
	BufferInterleaved							mBufferInterleaved;
	std::unique_ptr<VoiceCallbackImpl>			mVoiceCallback;

	friend class ContextXAudio;
};

class EffectXAudioXapo : public EffectNode, public NodeXAudio {
public:
	//! These enum names match the class uuid names in xapofx.h. TODO: consider just passing in the REFCLSID
	enum XapoType { FXEcho, FXEQ, FXMasteringLimiter, FXReverb };

	EffectXAudioXapo( XapoType type, const Format &format = Format() );
	virtual ~EffectXAudioXapo();

	std::string virtual getTag()				{ return "EffectXAudioXapo"; }

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

	EffectXAudioFilter( const Format &format = Format() );
	virtual ~EffectXAudioFilter();

	std::string virtual getTag()				{ return "EffectXAudioFilter"; }

	void initialize() override;
	void uninitialize() override;

	void getParams( ::XAUDIO2_FILTER_PARAMETERS *params );
	void setParams( const ::XAUDIO2_FILTER_PARAMETERS &params ); // TODO: ask AFB if this shoul be * of &

private:

};

class ContextXAudio : public Context {
  public:
	virtual ~ContextXAudio();

	ContextRef		createContext() override;
	LineOutNodeRef	createLineOut( DeviceRef device, const Node::Format &format = Node::Format() ) override;
	//! If deployment target is 0x601 (win vista) or greater, uses InputWasapi, else returns an empty DeviceRef
	LineInNodeRef	createLineIn( DeviceRef device, const Node::Format &format = Node::Format()  ) override;
	MixerNodeRef	createMixer( const Node::Format &format = Node::Format() ) override;

	void initialize() override;
	void uninitialize() override;
	void connectionsDidChange( const NodeRef &node ) override; 

	//! ContextXAudio's \a RootNode is always an instance of LineOutXAudio
	virtual RootNodeRef getRoot() override;

	IXAudio2* getXAudio();

  private:

	void initNode( NodeRef node );
	void uninitNode( NodeRef node );
	void setContext( NodeRef node );
	void initEffects( NodeRef node );
};

} } } // namespace cinder::audio2::msw