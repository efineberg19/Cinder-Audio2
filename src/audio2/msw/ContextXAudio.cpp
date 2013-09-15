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

#include "audio2/msw/ContextXAudio.h"
#include "audio2/msw/NodeLineInWasapi.h"
#include "audio2/msw/DeviceManagerWasapi.h"
#include "audio2/msw/xaudio.h"

#include "audio2/audio.h"
#include "audio2/CinderAssert.h"
#include "audio2/Debug.h"
#include "audio2/Dsp.h"

#include "cinder/Utilities.h"

using namespace std;

namespace cinder { namespace audio2 { namespace msw {

static bool isNodeNativeXAudio( NodeRef node ) {
	return dynamic_pointer_cast<NodeXAudio>( node );
}

struct VoiceCallbackImpl : public ::IXAudio2VoiceCallback {

	VoiceCallbackImpl( const function<void()> &callback  ) : mRenderCallback( callback ) {}

	void setInputVoice( ::IXAudio2SourceVoice *sourceVoice )	{ mSourceVoice = sourceVoice; }

	// TODO: apparently passing in XAUDIO2_VOICE_NOSAMPLESPLAYED to GetState is 4x faster
	void _stdcall OnBufferEnd( void *pBufferContext ) {
		::XAUDIO2_VOICE_STATE state;
		mSourceVoice->GetState( &state );
		if( state.BuffersQueued == 0 ) // This could be increased to 1 to decrease chances of underuns
			mRenderCallback();
	}

	void _stdcall OnStreamEnd() {}
	void _stdcall OnVoiceProcessingPassEnd() {}
	void _stdcall OnVoiceProcessingPassStart( UINT32 SamplesRequired ) {}
	void _stdcall OnBufferStart( void *pBufferContext ) {}
	void _stdcall OnLoopEnd( void *pBufferContext ) {}
	void _stdcall OnVoiceError( void *pBufferContext, HRESULT Error )
	{
		LOG_E << "error: " << Error << endl;
	}

	::IXAudio2SourceVoice	*mSourceVoice;
	function<void()>		mRenderCallback;
};

struct EngineCallbackImpl : public IXAudio2EngineCallback {
	EngineCallbackImpl( NodeLineOutXAudio *lineOut ) : mLineOut( lineOut ), mFramesPerBlock( lineOut->getFramesPerBlock() ) {}

	void _stdcall OnProcessingPassStart() {}
	void _stdcall OnProcessingPassEnd ()
	{
		mLineOut->mProcessedFrames += mFramesPerBlock;
	}
	void _stdcall OnCriticalError( HRESULT Error )
	{
		LOG_E << "error: " << Error << endl;
	}

	NodeLineOutXAudio *mLineOut;
	uint64_t mFramesPerBlock;
};

// ----------------------------------------------------------------------------------------------------
// MARK: - XAudioNode
// ----------------------------------------------------------------------------------------------------

NodeXAudio::~NodeXAudio()
{
}

// ----------------------------------------------------------------------------------------------------
// MARK: - LineOutXAudio
// ----------------------------------------------------------------------------------------------------

NodeLineOutXAudio::NodeLineOutXAudio( DeviceRef device, const Format &format )
: NodeLineOut( device, format ), mProcessedFrames( 0 ), mEngineCallback( new EngineCallbackImpl( this ) )
{
#if defined( CINDER_XAUDIO_2_7 )
	LOG_V << "CINDER_XAUDIO_2_7, toolset: v110_xp" << endl;
	UINT32 flags = XAUDIO2_DEBUG_ENGINE;

	ci::msw::initializeCom();

#else
	LOG_V << "CINDER_XAUDIO_2_8, toolset: v110" << endl;
	UINT32 flags = 0;
#endif

	HRESULT hr = ::XAudio2Create( &mXAudio, flags, XAUDIO2_DEFAULT_PROCESSOR );
	CI_ASSERT( hr == S_OK );
	hr = mXAudio->RegisterForCallbacks( mEngineCallback.get() );
	CI_ASSERT( hr == S_OK );

#if defined( CINDER_XAUDIO_2_8 )
	::XAUDIO2_DEBUG_CONFIGURATION debugConfig = {0};
	debugConfig.TraceMask = XAUDIO2_LOG_ERRORS;
	debugConfig.BreakMask = XAUDIO2_LOG_ERRORS;
	debugConfig.LogFunctionName = true;
	mXAudio->SetDebugConfiguration( &debugConfig );
#endif

	// mXAudio is started at creation time, so stop it here as mEnabled = false
	mXAudio->StopEngine();
}

NodeLineOutXAudio::~NodeLineOutXAudio()
{
	mXAudio->Release();
}

void NodeLineOutXAudio::initialize()
{
	auto deviceManager = dynamic_cast<DeviceManagerWasapi *>( Context::deviceManager() );
	const wstring &deviceId = deviceManager->getDeviceId( mDevice->getKey() );
	//const string &name = mDevice->getName();
	IXAudio2 *xaudio = dynamic_pointer_cast<ContextXAudio>( getContext() )->getXAudio();

#if defined( CINDER_XAUDIO_2_8 )
	// TODO: use mNumChannels / context sr
	HRESULT hr = xaudio->CreateMasteringVoice( &mMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, deviceId.c_str() );
	CI_ASSERT( hr == S_OK );
#else

	// TODO: on XAudio2.7, mKey (from WASAPI) is the device Id, but this isn't obvious below.  Consider re-mapping getDeviceId() to match this.
	UINT32 deviceCount;
	hr = mXAudio->GetDeviceCount( &deviceCount );
	CI_ASSERT( hr == S_OK );
	::XAUDIO2_DEVICE_DETAILS deviceDetails;
	for( UINT32 i = 0; i < deviceCount; i++ ) {
		hr = mXAudio->GetDeviceDetails( i, &deviceDetails );
		CI_ASSERT( hr == S_OK );
		if( mKey == ci::toUtf8( deviceDetails.DeviceID ) ) {
			LOG_V << "found match: display name: " << deviceDetails.DisplayName << endl;
			LOG_V << "device id: " << deviceDetails.DeviceID << endl;

			hr = mXAudio->CreateMasteringVoice( &mMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, i );
			CI_ASSERT( hr == S_OK );
		}

	}

	CI_ASSERT( mMasteringVoice );

#endif

	::XAUDIO2_VOICE_DETAILS voiceDetails;
	mMasteringVoice->GetVoiceDetails( &voiceDetails );
	LOG_V << "created mastering voice. channels: " << voiceDetails.InputChannels << ", samplerate: " << voiceDetails.InputSampleRate << endl;

	// normally mInitialized is handled via initializeImpl(), but SourceVoiceXaudio
	// needs to ensure this guy is around before it can do anything and it can't call
	// Node::initializeImpl(). So instead the flag is manually updated.
	mInitialized = true;
}

void NodeLineOutXAudio::uninitialize()
{
	CI_ASSERT_MSG( mMasteringVoice, "Expected to have a valid mastering voice" );
	mMasteringVoice->DestroyVoice();
}

void NodeLineOutXAudio::start()
{
	if( mEnabled || ! mInitialized )
		return;

	mEnabled = true;

	HRESULT hr = mXAudio->StartEngine();
	CI_ASSERT( hr ==S_OK );
	LOG_V "started" << endl;
}

void NodeLineOutXAudio::stop()
{
	if( ! mEnabled || ! mInitialized )
		return;

	mEnabled = false;
	mXAudio->StopEngine();
	LOG_V "stopped" << endl;
}

bool NodeLineOutXAudio::supportsSourceNumChannels( size_t numChannels ) const
{
	return true;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - SourceVoiceXAudio
// ----------------------------------------------------------------------------------------------------

NodeXAudioSourceVoice::NodeXAudioSourceVoice()
: Node( Format() )
{
	setAutoEnabled( true );
	mVoiceCallback = unique_ptr<VoiceCallbackImpl>( new VoiceCallbackImpl( bind( &NodeXAudioSourceVoice::submitNextBuffer, this ) ) );
}

NodeXAudioSourceVoice::~NodeXAudioSourceVoice()
{
}

void NodeXAudioSourceVoice::initialize()
{
	// TODO: handle higher channel counts
	// - use case: LineIn connected to 4 microphones
	CI_ASSERT( getNumChannels() <= 2 ); 

	setProcessWithSumming();
	size_t numSamples = mInternalBuffer.getSize();

	memset( &mXAudio2Buffer, 0, sizeof( mXAudio2Buffer ) );
	mXAudio2Buffer.AudioBytes = numSamples * sizeof( float );
	if( getNumChannels() == 2 ) {
		// setup stereo, XAudio2 requires interleaved samples so point at interleaved buffer
		mBufferInterleaved = BufferInterleaved( mInternalBuffer.getNumFrames(), mInternalBuffer.getNumChannels() );
		mXAudio2Buffer.pAudioData = reinterpret_cast<BYTE *>( mBufferInterleaved.getData() );
	}
	else {
		// setup mono
		mXAudio2Buffer.pAudioData = reinterpret_cast<BYTE *>( mInternalBuffer.getData() );
	}

	initSourceVoice();
}

void NodeXAudioSourceVoice::uninitialize()
{
	uninitSourceVoice();
	// FIXME: looks like SourceVoice senders need to be detached during its uninitialize, which was attached during CreateSourceVoice
}

void NodeXAudioSourceVoice::initSourceVoice()
{
	ContextRef context = getContext();

	// first ensure there is a valid mastering voice.
	NodeTargetRef target = context->getTarget();
	if( ! target->isInitialized() )
		target->initialize();

	auto wfx = msw::interleavedFloatWaveFormat( getNumChannels(), context->getSampleRate() );

	IXAudio2 *xaudio = dynamic_pointer_cast<ContextXAudio>( getContext() )->getXAudio();
	UINT32 flags = ( mFilterEnabled ? XAUDIO2_VOICE_USEFILTER : 0 );
	HRESULT hr = xaudio->CreateSourceVoice( &mSourceVoice, wfx.get(), flags, XAUDIO2_DEFAULT_FREQ_RATIO, mVoiceCallback.get()  );
	CI_ASSERT( hr == S_OK );
	mVoiceCallback->setInputVoice( mSourceVoice );
}

void NodeXAudioSourceVoice::uninitSourceVoice()
{
	if( mSourceVoice ) {
		mSourceVoice->DestroyVoice();
		mSourceVoice = nullptr;
	}
}

// TODO: source voice must be made during initialize() pass, so there is a chance start/stop can be called
// before. Decide on throwing, silently failing, or a something better.
void NodeXAudioSourceVoice::start()
{
	if( mEnabled )
		return;

	CI_ASSERT( mSourceVoice );
	mEnabled = true;
	mSourceVoice->Start();
	submitNextBuffer();

	LOG_V << "started." << endl;
}

void NodeXAudioSourceVoice::stop()
{
	if( ! mEnabled )
		return;

	CI_ASSERT( mSourceVoice );
	mEnabled = false;
	mSourceVoice->Stop();
	LOG_V << "stopped." << endl;
}

void NodeXAudioSourceVoice::submitNextBuffer()
{
	lock_guard<mutex> lock( getContext()->getMutex() );

	mInternalBuffer.zero();
	pullInputs( &mInternalBuffer );

	if( getNumChannels() == 2 )
		interleaveStereoBuffer( &mInternalBuffer, &mBufferInterleaved );

	HRESULT hr = mSourceVoice->SubmitSourceBuffer( &mXAudio2Buffer );
	CI_ASSERT( hr == S_OK );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - EffectXAudioXapo
// ----------------------------------------------------------------------------------------------------

// TODO: cover the 2 built-ins too, included via xaudio2fx.h
NodeEffectXAudioXapo::NodeEffectXAudioXapo( XapoType type, const Format &format )
: NodeEffect( format ), mType( type )
{
	switch( type ) {
		case XapoType::FXEcho:				makeXapo( __uuidof( ::FXEcho ) ); break;
		case XapoType::FXEQ:				makeXapo( __uuidof( ::FXEQ ) ); break;
		case XapoType::FXMasteringLimiter:	makeXapo( __uuidof( ::FXMasteringLimiter ) ); break;
		case XapoType::FXReverb:			makeXapo( __uuidof( ::FXReverb ) ); break;
	}

	mEffectDesc.InitialState = true;
	mEffectDesc.pEffect = mXapo.get();
}

NodeEffectXAudioXapo::~NodeEffectXAudioXapo()
{
}

void NodeEffectXAudioXapo::initialize()
{
	mEffectDesc.OutputChannels = getNumChannels();
}

void NodeEffectXAudioXapo::makeXapo( REFCLSID clsid )
{
	::IUnknown *xapo;
	HRESULT hr = ::CreateFX( clsid, &xapo );
	CI_ASSERT( hr == S_OK );
	mXapo = msw::makeComUnique( xapo );
}

void NodeEffectXAudioXapo::notifyConnected()
{
	CI_ASSERT( mInitialized );

	auto sourceVoice = findDownStreamNode<NodeXAudioSourceVoice>( shared_from_this() );
	auto &effects = sourceVoice->getEffectsDescriptors();
	mChainIndex = effects.size();
	if( mChainIndex > 0 ) {
		// An effect has already been connected and thereby SetEffectsChain has already been called.
		// As it seems this can only be called once for the lifetime of an IXAUdio2SourceVoice, we re-init
		LOG_V << "sourceVoice re-init, mChainIndex: " << mChainIndex << endl;
		sourceVoice->uninitSourceVoice();
		sourceVoice->initSourceVoice();
	}

	effects.push_back( mEffectDesc );

	::XAUDIO2_EFFECT_CHAIN effectsChain;
	effectsChain.EffectCount = effects.size();
	effectsChain.pEffectDescriptors = effects.data();

	LOG_V << "SetEffectChain, p: " << (void*)sourceVoice->getNative() << ", count: " << effectsChain.EffectCount << endl;
	HRESULT hr = sourceVoice->getNative()->SetEffectChain( &effectsChain );
	CI_ASSERT( hr == S_OK );
}

void NodeEffectXAudioXapo::getParams( void *params, size_t sizeParams )
{
	if( ! mInitialized )
		throw AudioParamExc( "must be initialized before accessing params" );

	auto sourceVoice = findDownStreamNode<NodeXAudioSourceVoice>( shared_from_this() );
	HRESULT hr = sourceVoice->getNative()->GetEffectParameters( mChainIndex, params, sizeParams );
	CI_ASSERT( hr == S_OK );
}

void NodeEffectXAudioXapo::setParams( const void *params, size_t sizeParams )
{
	if( ! mInitialized )
		throw AudioParamExc( "must be initialized before accessing params" );

	auto sourceVoice = findDownStreamNode<NodeXAudioSourceVoice>( shared_from_this() );
	HRESULT hr =  sourceVoice->getNative()->SetEffectParameters( mChainIndex, params, sizeParams );
	CI_ASSERT( hr == S_OK );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - EffectXAudioFilter
// ----------------------------------------------------------------------------------------------------

NodeEffectXAudioFilter::NodeEffectXAudioFilter( const Format &format )
: NodeEffect( format )
{
}

NodeEffectXAudioFilter::~NodeEffectXAudioFilter()
{

}

void NodeEffectXAudioFilter::initialize()
{
}

void NodeEffectXAudioFilter::uninitialize()
{
}

void NodeEffectXAudioFilter::getParams( ::XAUDIO2_FILTER_PARAMETERS *params )
{
	if( ! mInitialized )
		throw AudioParamExc( "must be initialized before accessing params" );

	auto sourceVoice = findDownStreamNode<NodeXAudioSourceVoice>( shared_from_this() );
	sourceVoice->getNative()->GetFilterParameters( params );
}

void NodeEffectXAudioFilter::setParams( const ::XAUDIO2_FILTER_PARAMETERS &params )
{
	if( ! mInitialized )
		throw AudioParamExc( "must be initialized before accessing params" );

	auto sourceVoice = findDownStreamNode<NodeXAudioSourceVoice>( shared_from_this() );
	HRESULT hr = sourceVoice->getNative()->SetFilterParameters( &params );
	CI_ASSERT( hr == S_OK );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - ContextXAudio
// ----------------------------------------------------------------------------------------------------

ContextXAudio::ContextXAudio()
{
}

ContextXAudio::~ContextXAudio()
{
}

NodeLineOutRef ContextXAudio::createLineOut( DeviceRef device, const Node::Format &format )
{
	return makeNode( new NodeLineOutXAudio( device, format ) );
}

NodeLineInRef ContextXAudio::createLineIn( DeviceRef device, const Node::Format &format )
{
	return makeNode( new NodeLineInWasapi( device ) );
}

void ContextXAudio::connectionsDidChange( const NodeRef &node )
{
	// recurse through inputs (this is only called when an input is set, not out)
	for( size_t i = 0; i < node->getInputs().size(); i++ ) {
		NodeRef input = node->getInputs()[i];
		if( ! input )
			continue;

		// if input is generic, it needs a SourceXAudio so add one implicitly
		if( ! isNodeNativeXAudio( input ) ) {
			shared_ptr<NodeXAudioSourceVoice> sourceVoice = findUpstreamNode<NodeXAudioSourceVoice>( input );
			if( ! sourceVoice ) {
				// see if there is already a downstream source voice
				sourceVoice = findDownStreamNode<NodeXAudioSourceVoice>( input );
				if( sourceVoice ) {
					LOG_V << "detected downstream source node, shuffling." << endl;
					// FIXME: account account for multiple inputs in both input and sourceVoice
					NodeRef sourceInput = sourceVoice->getInputs()[0];
					sourceVoice->disconnect();
					node->setInput( sourceVoice, i );
					sourceVoice->setInput( input );
					input->setInput( sourceInput, 0 );
				}
				else if( findDownStreamNode<NodeXAudio>( input ) )
					throw AudioContextExc( "Detected generic node after native Xapo, custom Xapo's not implemented." );
				else {
					LOG_V << "implicit connection: " << input->getTag() << " -> SourceVoiceXAudio -> " << node->getTag() << endl;

					sourceVoice = makeNode( new NodeXAudioSourceVoice() );
					sourceVoice->setNumChannels( input->getNumChannels() ); // TODO: this probably isn't necessary, should be taken care of in setInput if format is setup correct
					sourceVoice->setFilterEnabled(); // TODO: detect if there is an effect upstream before enabling filters
					sourceVoice->initialize();

					node->setInput( sourceVoice, i );
					sourceVoice->setInput( input );
				}
			}
			continue;
		}
		
		shared_ptr<NodeEffectXAudioXapo> xapo = dynamic_pointer_cast<NodeEffectXAudioXapo>( input );
		if( xapo )
			xapo->notifyConnected();
	}
}

} } } // namespace cinder::audio2::msw