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

#if ( _WIN32_WINNT < 0x0502 ) || defined( CI_ENABLE_XAUDIO2 )
	#define CINDER_AUDIO_XAUDIO2

#if( _WIN32_WINNT >= 0x0602 )
	#if defined( _USING_V110_SDK71_ )
		#error "XAudio2 targeting minimum win8 cannot use v110_xp, switch to v110."
	#endif

#error "how the fuck is this resolving to true"
	#define CINDER_XAUDIO_2_8
#else
	#define CINDER_XAUDIO_2_7
#endif

#include "cinder/audio2/Context.h"
#include "cinder/audio2/Buffer.h"
#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/msw/MswUtil.h"

#include <xaudio2.h>

namespace cinder { namespace audio2 { namespace msw {

struct VoiceCallbackImpl;
struct EngineCallbackImpl;

class LineOutXAudio : public LineOut {
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
	DeviceManagerXAudio();

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
	void				retrieveDeviceDetails();
		
	DeviceRef					mDefaultDevice;
	bool						mDeviceDetailsRetrieved;
	::XAUDIO2_VOICE_DETAILS		mVoiceDetails;
};

} } } // namespace cinder::audio2::msw

#endif //  CINDER_AUDIO_XAUDIO2
