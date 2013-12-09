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

#include "cinder/audio2/msw/ContextXAudio.h"
#include "cinder/audio2/msw/LineInWasapi.h"
#include "cinder/audio2/msw/DeviceManagerWasapi.h"
#include "cinder/audio2/msw/xaudio.h"

#include "cinder/audio2/audio.h"
#include "cinder/audio2/CinderAssert.h"
#include "cinder/audio2/Debug.h"
#include "cinder/audio2/dsp/Dsp.h"

#include "cinder/Utilities.h"

using namespace std;

namespace cinder { namespace audio2 { namespace msw {

static bool isNodeNativeXAudio( NodeRef node ) {
	return dynamic_pointer_cast<NodeXAudio>( node );
}

struct VoiceCallbackImpl : public ::IXAudio2VoiceCallback {

	VoiceCallbackImpl( const function<void()> &callback  ) : mRenderCallback( callback ) {}

	void setInputVoice( ::IXAudio2SourceVoice *sourceVoice )	{ mSourceVoice = sourceVoice; }
	void renderIfNecessary()
	{
		::XAUDIO2_VOICE_STATE state;
		mSourceVoice->GetState( &state, XAUDIO2_VOICE_NOSAMPLESPLAYED );
		if( state.BuffersQueued == 0 ) // This could be increased to 1 to decrease chances of underuns
			mRenderCallback();
	}

	void _stdcall OnBufferEnd( void *pBufferContext )
	{
		// called when a buffer has been consumed, so check if we need to submit another
		renderIfNecessary();
	}

	void _stdcall OnVoiceProcessingPassStart( UINT32 SamplesRequired )
	{
		// called after Node::start(), and there after
		renderIfNecessary();
	}

	void _stdcall OnStreamEnd() {}
	void _stdcall OnVoiceProcessingPassEnd() {}
	void _stdcall OnBufferStart( void *pBufferContext ) {}
	void _stdcall OnLoopEnd( void *pBufferContext ) {}
	void _stdcall OnVoiceError( void *pBufferContext, HRESULT Error )
	{
		LOG_E( "error: " << Error );
	}

	::IXAudio2SourceVoice	*mSourceVoice;
	function<void()>		mRenderCallback;
};

struct EngineCallbackImpl : public IXAudio2EngineCallback {
	EngineCallbackImpl( LineOutXAudio *lineOut ) : mLineOut( lineOut ), mFramesPerBlock( lineOut->getFramesPerBlock() ) {}

	void _stdcall OnProcessingPassStart() {}
	void _stdcall OnProcessingPassEnd ()
	{
		mLineOut->mProcessedFrames += mFramesPerBlock;
	}
	void _stdcall OnCriticalError( HRESULT Error )
	{
		LOG_E( "error: " << Error );
	}

	LineOutXAudio *mLineOut;
	uint64_t mFramesPerBlock;
};

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeXAudio
// ----------------------------------------------------------------------------------------------------

NodeXAudio::~NodeXAudio()
{
}


// ----------------------------------------------------------------------------------------------------
// MARK: - SourceVoiceXAudio
// ----------------------------------------------------------------------------------------------------

NodeXAudioSourceVoice::NodeXAudioSourceVoice()
	: Node( Format() ), mSourceVoice( nullptr )
{
	setAutoEnabled( true );
	mVoiceCallback = unique_ptr<VoiceCallbackImpl>( new VoiceCallbackImpl( bind( &NodeXAudioSourceVoice::submitNextBuffer, this ) ) );
}

NodeXAudioSourceVoice::~NodeXAudioSourceVoice()
{
	uninitSourceVoice();
}

void NodeXAudioSourceVoice::initialize()
{
	// TODO: handle higher channel counts
	// - use case: LineIn connected to 4 microphones
	CI_ASSERT( getNumChannels() <= 2 ); 

	setupProcessWithSumming();
	size_t numSamples = mInternalBuffer.getSize();

	memset( &mXAudioBuffer, 0, sizeof( mXAudioBuffer ) );
	mXAudioBuffer.AudioBytes = numSamples * sizeof( float );
	if( getNumChannels() == 2 ) {
		// setup stereo, XAudio2 requires interleaved samples so point at interleaved buffer
		mBufferInterleaved = BufferInterleaved( mInternalBuffer.getNumFrames(), mInternalBuffer.getNumChannels() );
		mXAudioBuffer.pAudioData = reinterpret_cast<BYTE *>( mBufferInterleaved.getData() );
	}
	else {
		// setup mono
		mXAudioBuffer.pAudioData = reinterpret_cast<BYTE *>( mInternalBuffer.getData() );
	}

	initSourceVoice();

	// ContextXAudio may call this method, so make sure to update flag so it is seen
	mInitialized = true;
}

void NodeXAudioSourceVoice::uninitialize()
{
	uninitSourceVoice();
}

void NodeXAudioSourceVoice::initSourceVoice()
{
	CI_ASSERT( ! mSourceVoice );

	auto context = dynamic_pointer_cast<ContextXAudio>( getContext() );

	// first ensure there is a valid mastering voice.
	NodeTargetRef target = context->getTarget();
	if( ! target->isInitialized() )
		target->initialize();

	auto wfx = msw::interleavedFloatWaveFormat( context->getSampleRate(), getNumChannels() );

	IXAudio2 *xaudio = context->getXAudio();
	UINT32 flags = ( context->isFilterEffectsEnabled() ? XAUDIO2_VOICE_USEFILTER : 0 );
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

	LOG_V( "started." );
}

void NodeXAudioSourceVoice::stop()
{
	if( ! mEnabled )
		return;

	CI_ASSERT( mSourceVoice );
	mEnabled = false;
	mSourceVoice->Stop();
	LOG_V( "stopped." );
}

void NodeXAudioSourceVoice::submitNextBuffer()
{
	lock_guard<mutex> lock( getContext()->getMutex() );

	mInternalBuffer.zero();
	pullInputs( &mInternalBuffer );

	if( getNumChannels() == 2 )
		interleaveStereoBuffer( &mInternalBuffer, &mBufferInterleaved );

	HRESULT hr = mSourceVoice->SubmitSourceBuffer( &mXAudioBuffer );
	CI_ASSERT( hr == S_OK );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - LineOutXAudio
// ----------------------------------------------------------------------------------------------------

LineOutXAudio::LineOutXAudio( DeviceRef device, const Format &format )
: LineOut( device, format ), mProcessedFrames( 0 ), mEngineCallback( new EngineCallbackImpl( this ) )
{
#if defined( CINDER_XAUDIO_2_7 )
	LOG_V( "CINDER_XAUDIO_2_7, toolset: v110_xp" );
	UINT32 flags = XAUDIO2_DEBUG_ENGINE;

	ci::msw::initializeCom();

#else
	LOG_V( "CINDER_XAUDIO_2_8, toolset: v110" );
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

LineOutXAudio::~LineOutXAudio()
{
	mXAudio->Release();
}

void LineOutXAudio::initialize()
{
	auto deviceManager = dynamic_cast<DeviceManagerWasapi *>( Context::deviceManager() );
	const wstring &deviceId = deviceManager->getDeviceId( mDevice );
	//const string &name = mDevice->getName();
	IXAudio2 *xaudio = dynamic_pointer_cast<ContextXAudio>( getContext() )->getXAudio();

#if defined( CINDER_XAUDIO_2_8 )
	HRESULT hr = xaudio->CreateMasteringVoice( &mMasteringVoice, getNumChannels(), getSampleRate(), 0, deviceId.c_str() );
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
			LOG_V( "found match: display name: " << deviceDetails.DisplayName );
			LOG_V( "device id: " << deviceDetails.DeviceID );

			hr = mXAudio->CreateMasteringVoice( &mMasteringVoice,  getNumChannels(), getSampleRate(), 0, i );
			CI_ASSERT( hr == S_OK );
		}

	}

	CI_ASSERT( mMasteringVoice );

#endif

	::XAUDIO2_VOICE_DETAILS voiceDetails;
	mMasteringVoice->GetVoiceDetails( &voiceDetails );
	LOG_V( "created mastering voice. channels: " << voiceDetails.InputChannels << ", samplerate: " << voiceDetails.InputSampleRate );

	// normally mInitialized is handled via initializeImpl(), but SourceVoiceXaudio
	// needs to ensure this guy is around before it can do anything and it can't call
	// Node::initializeImpl(). So instead the flag is manually updated.
	mInitialized = true;
}

void LineOutXAudio::uninitialize()
{
	CI_ASSERT_MSG( mMasteringVoice, "Expected to have a valid mastering voice" );
	mMasteringVoice->DestroyVoice();
}

void LineOutXAudio::start()
{
	if( mEnabled || ! mInitialized )
		return;

	mEnabled = true;

	HRESULT hr = mXAudio->StartEngine();
	CI_ASSERT( hr ==S_OK );
	LOG_V( "started" );
}

void LineOutXAudio::stop()
{
	if( ! mEnabled || ! mInitialized )
		return;

	mEnabled = false;
	mXAudio->StopEngine();
	LOG_V( "stopped" );
}

uint64_t LineOutXAudio::getLastClip()
{
	return 0; // TODO: set clip frame from source nodes
}

bool LineOutXAudio::supportsInputNumChannels( size_t numChannels )
{
	return true;
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

	auto sourceVoice = findFirstUpstreamNode<NodeXAudioSourceVoice>( shared_from_this() );
	auto &effects = sourceVoice->getEffectsDescriptors();
	mChainIndex = effects.size();
	if( mChainIndex > 0 ) {
		// An effect has already been connected and thereby SetEffectsChain has already been called.
		// As it seems this can only be called once for the lifetime of an IXAUdio2SourceVoice, we re-init
		LOG_V( "sourceVoice re-init, mChainIndex: " << mChainIndex );
		sourceVoice->uninitSourceVoice();
		sourceVoice->initSourceVoice();
	}

	effects.push_back( mEffectDesc );

	::XAUDIO2_EFFECT_CHAIN effectsChain;
	effectsChain.EffectCount = effects.size();
	effectsChain.pEffectDescriptors = effects.data();

	LOG_V( "SetEffectChain, p: " << (void*)sourceVoice->getNative() << ", count: " << effectsChain.EffectCount );
	HRESULT hr = sourceVoice->getNative()->SetEffectChain( &effectsChain );
	CI_ASSERT( hr == S_OK );
}

void NodeEffectXAudioXapo::getParams( void *params, size_t sizeParams )
{
	if( ! mInitialized )
		throw AudioParamExc( "must be initialized before accessing params" );

	auto sourceVoice = findFirstUpstreamNode<NodeXAudioSourceVoice>( shared_from_this() );
	HRESULT hr = sourceVoice->getNative()->GetEffectParameters( mChainIndex, params, sizeParams );
	CI_ASSERT( hr == S_OK );
}

void NodeEffectXAudioXapo::setParams( const void *params, size_t sizeParams )
{
	if( ! mInitialized )
		throw AudioParamExc( "must be initialized before accessing params" );

	auto sourceVoice = findFirstUpstreamNode<NodeXAudioSourceVoice>( shared_from_this() );
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

	auto sourceVoice = findFirstUpstreamNode<NodeXAudioSourceVoice>( shared_from_this() );
	sourceVoice->getNative()->GetFilterParameters( params );
}

void NodeEffectXAudioFilter::setParams( const ::XAUDIO2_FILTER_PARAMETERS &params )
{
	if( ! mInitialized )
		throw AudioParamExc( "must be initialized before accessing params" );

	auto sourceVoice = findFirstUpstreamNode<NodeXAudioSourceVoice>( shared_from_this() );
	HRESULT hr = sourceVoice->getNative()->SetFilterParameters( &params );
	CI_ASSERT( hr == S_OK );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - ContextXAudio
// ----------------------------------------------------------------------------------------------------

ContextXAudio::ContextXAudio()
: mFilterEnabled( true )
{
}

ContextXAudio::~ContextXAudio()
{
}

LineOutRef ContextXAudio::createLineOut( const DeviceRef &device, const Node::Format &format )
{
	return makeNode( new LineOutXAudio( device, format ) );
}

LineInRef ContextXAudio::createLineIn( const DeviceRef &device, const Node::Format &format )
{
	return makeNode( new LineInWasapi( device ) );
}

void ContextXAudio::setTarget( const NodeTargetRef &target )
{
	CI_ASSERT_MSG( dynamic_pointer_cast<LineOutXAudio>( target ), "ContextXAudio only supports a target of type LineOutXAudio" );
	Context::setTarget( target );
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
			shared_ptr<NodeXAudioSourceVoice> sourceVoice = findFirstDownstreamNode<NodeXAudioSourceVoice>( input );
			if( ! sourceVoice ) {
				// see if there is already an upstream source voice
				sourceVoice = findFirstUpstreamNode<NodeXAudioSourceVoice>( input );
				if( sourceVoice ) {
					LOG_V( "detected upstream source node, shuffling." );
					// TODO: account account for multiple inputs in both input and sourceVoice
					NodeRef sourceInput = sourceVoice->getInputs()[0];
					sourceVoice->disconnect();

					//node->setInput( sourceVoice, i );
					//sourceVoice->setInput( input, 0 );
					//input->setInput( sourceInput, 0 );

					sourceInput->connect( input );
					input->connect( sourceVoice );
					sourceVoice->connect( node, 0, i );

				}
				else if( findFirstUpstreamNode<NodeXAudio>( input ) )
					throw AudioContextExc( "Detected generic node after native Xapo, custom Xapo's not implemented." );
				else {
					LOG_V( "implicit connection: " << input->getName() << " -> SourceVoiceXAudio -> " << node->getName() );

					sourceVoice = makeNode( new NodeXAudioSourceVoice() );

					//node->setInput( sourceVoice, i );
					//sourceVoice->setInput( input );

					input->connect( sourceVoice );
					sourceVoice->connect( node, 0, i );

					sourceVoice->start();
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