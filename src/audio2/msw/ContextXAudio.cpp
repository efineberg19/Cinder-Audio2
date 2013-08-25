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
#include "audio2/msw/DeviceOutputXAudio.h"
#include "audio2/msw/DeviceInputWasapi.h"

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

static shared_ptr<NodeXAudio> getXAudioNode( NodeRef node )
{
	while( node ) {
		auto nodeXAudio = dynamic_pointer_cast<NodeXAudio>( node );
		if( nodeXAudio )
			return nodeXAudio;
		else if( node->getInputs().empty() )
			break;

		node = node->getInputs().front();
	}
	LOG_V << "No XAudioNode in this tree." << endl;
	return shared_ptr<NodeXAudio>();
}

//! SourceVoiceXAudio resides downstream from all native nodes, so we fallow outputs to see if we can find one
static shared_ptr<SourceVoiceXAudio> getSourceVoice( NodeRef node )
{
	CI_ASSERT( node );
	while( node ) {
		auto sourceVoice = dynamic_pointer_cast<SourceVoiceXAudio>( node );
		if( sourceVoice )
			return sourceVoice;
		node = node->getOutput();
	}
	LOG_V << "No SourceVoiceXAudio in this tree." << endl;
	return shared_ptr<SourceVoiceXAudio>();
}

struct VoiceCallbackImpl : public ::IXAudio2VoiceCallback {

	VoiceCallbackImpl( const function<void()> &callback  ) : mRenderCallback( callback ) {}

	void setInputVoice( ::IXAudio2SourceVoice *sourceVoice )	{ mSourceVoice = sourceVoice; }

	// TODO: apparently passing in XAUDIO2_VOICE_NOSAMPLESPLAYED to GetState is 4x faster
	void STDMETHODCALLTYPE OnBufferEnd( void *pBufferContext ) {
		::XAUDIO2_VOICE_STATE state;
		mSourceVoice->GetState( &state );
		if( state.BuffersQueued == 0 ) // This could be increased to 1 to decrease chances of underuns
			mRenderCallback();
	}

	void STDMETHODCALLTYPE OnStreamEnd() {}
	void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() {}
	void STDMETHODCALLTYPE OnVoiceProcessingPassStart( UINT32 SamplesRequired ) {}
	void STDMETHODCALLTYPE OnBufferStart( void *pBufferContext ) {}
	void STDMETHODCALLTYPE OnLoopEnd( void *pBufferContext ) {}
	void STDMETHODCALLTYPE OnVoiceError( void *pBufferContext, HRESULT Error )	{ CI_ASSERT( false ); }

	::IXAudio2SourceVoice	*mSourceVoice;
	function<void()>		mRenderCallback;
};

// ----------------------------------------------------------------------------------------------------
// MARK: - XAudioNode
// ----------------------------------------------------------------------------------------------------

NodeXAudio::~NodeXAudio()
{
}

XAudioVoice NodeXAudio::getXAudioVoice( NodeRef node )
{
	CI_ASSERT( ! node->getInputs().empty() );
	NodeRef source = node->getInputs().front();

	auto sourceXAudio = dynamic_pointer_cast<NodeXAudio>( source );
	if( sourceXAudio )
		return sourceXAudio->getXAudioVoice( source );

	return getXAudioVoice( source );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - LineOutXAudio
// ----------------------------------------------------------------------------------------------------

LineOutXAudio::LineOutXAudio( DeviceRef device, const Format &format )
: LineOutNode( device, format )
{
	mDevice = dynamic_pointer_cast<DeviceOutputXAudio>( device );
	CI_ASSERT( mDevice );
}

void LineOutXAudio::initialize()
{
	// Device initialize is handled by the graph because it needs to ensure there is a valid IXAudio instance and mastering voice before anything else is initialized
	//mDevice->initialize();
	mInitialized = true;
}

void LineOutXAudio::uninitialize()
{
	mDevice->uninitialize();
	mInitialized = false;
}

void LineOutXAudio::start()
{
	mDevice->start();
	mEnabled = true;
	LOG_V << "started: " << mDevice->getName() << endl;
}

void LineOutXAudio::stop()
{
	mDevice->stop();
	mEnabled = false;
	LOG_V << "stopped: " << mDevice->getName() << endl;
}

DeviceRef LineOutXAudio::getDevice()
{
	return std::static_pointer_cast<Device>( mDevice );
}

bool LineOutXAudio::supportsSourceNumChannels( size_t numChannels ) const
{
	return true;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - SourceVoiceXAudio
// ----------------------------------------------------------------------------------------------------

// TODO: blocksize needs to be exposed.
SourceVoiceXAudio::SourceVoiceXAudio()
: Node( Format() )
{
	setAutoEnabled( true );
	mVoiceCallback = unique_ptr<VoiceCallbackImpl>( new VoiceCallbackImpl( bind( &SourceVoiceXAudio::submitNextBuffer, this ) ) );
}

SourceVoiceXAudio::~SourceVoiceXAudio()
{

}

void SourceVoiceXAudio::initialize()
{
	// TODO: consider whether this should handle higher channel counts, or disallow in graph configure / node format
	CI_ASSERT( getNumChannels() <= 2 ); 

	mBuffer = Buffer( getNumChannels(), getContext()->getNumFramesPerBlock() );
	
	size_t numSamples = mBuffer.getSize();

	memset( &mXAudio2Buffer, 0, sizeof( mXAudio2Buffer ) );
	mXAudio2Buffer.AudioBytes = numSamples * sizeof( float );
	if( getNumChannels() == 2 ) {
		// setup stereo, XAudio2 requires interleaved samples so point at interleaved buffer
		mBufferInterleaved = Buffer( mBuffer.getNumChannels(), mBuffer.getNumFrames(), Buffer::Layout::INTERLEAVED );
		mXAudio2Buffer.pAudioData = reinterpret_cast<BYTE *>( mBufferInterleaved.getData() );
	} else {
		// setup mono
		mXAudio2Buffer.pAudioData = reinterpret_cast<BYTE *>(  mBuffer.getData() );
	}

	auto wfx = msw::interleavedFloatWaveFormat( getNumChannels(), getContext()->getSampleRate() );

	IXAudio2 *xaudio = dynamic_pointer_cast<ContextXAudio>( getContext() )->getXAudio();
	UINT32 flags = ( mFilterEnabled ? XAUDIO2_VOICE_USEFILTER : 0 );
	HRESULT hr = xaudio->CreateSourceVoice( &mSourceVoice, wfx.get(), flags, XAUDIO2_DEFAULT_FREQ_RATIO, mVoiceCallback.get()  );
	CI_ASSERT( hr == S_OK );
	mVoiceCallback->setInputVoice( mSourceVoice );

	mInitialized = true;
	LOG_V << "complete." << endl;
}

void SourceVoiceXAudio::uninitialize()
{
	mInitialized = false;

	if( mSourceVoice )
		mSourceVoice->DestroyVoice();
}

// TODO: source voice must be made during initialize() pass, so there is a chance start/stop can be called
// before. Decide on throwing, silently failing, or a something better.
void SourceVoiceXAudio::start()
{
	CI_ASSERT( mSourceVoice );
	mEnabled = true;
	mSourceVoice->Start();
	submitNextBuffer();

	LOG_V << "started." << endl;
}

void SourceVoiceXAudio::stop()
{
	CI_ASSERT( mSourceVoice );
	mEnabled = false;
	mSourceVoice->Stop();
	LOG_V << "stopped." << endl;
}

void SourceVoiceXAudio::submitNextBuffer()
{
	CI_ASSERT( mSourceVoice );

	mBuffer.zero();
	renderNode( getInputs()[0] );

	if( getNumChannels() == 2 )
		interleaveStereoBuffer( &mBuffer, &mBufferInterleaved );

	HRESULT hr = mSourceVoice->SubmitSourceBuffer( &mXAudio2Buffer );
	CI_ASSERT( hr == S_OK );
}

void SourceVoiceXAudio::renderNode( NodeRef node )
{
	if( ! node->getInputs().empty() )
		renderNode( node->getInputs()[0] );

	if( node->isEnabled() )
		node->process( &mBuffer );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - EffectXAudioXapo
// ----------------------------------------------------------------------------------------------------

// TODO: cover the 2 built-ins too, included via xaudio2fx.h
EffectXAudioXapo::EffectXAudioXapo( XapoType type, const Format &format )
: EffectNode( format ), mType( type )
{
	switch( type ) {
		case XapoType::FXEcho:				makeXapo( __uuidof( ::FXEcho ) ); break;
		case XapoType::FXEQ:				makeXapo( __uuidof( ::FXEQ ) ); break;
		case XapoType::FXMasteringLimiter:	makeXapo( __uuidof( ::FXMasteringLimiter ) ); break;
		case XapoType::FXReverb:			makeXapo( __uuidof( ::FXReverb ) ); break;
	}
}

EffectXAudioXapo::~EffectXAudioXapo()
{
}

void EffectXAudioXapo::makeXapo( REFCLSID clsid )
{
	::IUnknown *xapo;
	HRESULT hr = ::CreateFX( clsid, &xapo );
	CI_ASSERT( hr == S_OK );
	mXapo = msw::makeComUnique( xapo );
}

void EffectXAudioXapo::initialize()
{
	::XAUDIO2_EFFECT_DESCRIPTOR effectDesc;
	//effectDesc.InitialState = mEnabled = true; // TODO: add enabled / running param for all Nodes.
	effectDesc.InitialState = true;
	effectDesc.pEffect = mXapo.get();
	effectDesc.OutputChannels = getNumChannels();

	XAudioVoice v = getXAudioVoice( shared_from_this() );
	auto &effects = v.node->getEffectsDescriptors();
	mChainIndex = effects.size();
	effects.push_back( effectDesc );

	mInitialized = true;
	LOG_V << "complete. effect index: " << mChainIndex << endl;
}

void EffectXAudioXapo::uninitialize()
{
	mInitialized = false;
}

void EffectXAudioXapo::getParams( void *params, size_t sizeParams )
{
	if( ! mInitialized )
		throw AudioParamExc( "must be initialized before accessing params" );

	XAudioVoice v = getXAudioVoice( shared_from_this() );
	HRESULT hr = v.voice->GetEffectParameters( mChainIndex, params, sizeParams );
	CI_ASSERT( hr == S_OK );
}

void EffectXAudioXapo::setParams( const void *params, size_t sizeParams )
{
	if( ! mInitialized )
		throw AudioParamExc( "must be initialized before accessing params" );

	XAudioVoice v = getXAudioVoice( shared_from_this() );
	HRESULT hr = v.voice->SetEffectParameters( mChainIndex, params, sizeParams );
	CI_ASSERT( hr == S_OK );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - EffectXAudioFilter
// ----------------------------------------------------------------------------------------------------

EffectXAudioFilter::EffectXAudioFilter( const Format &format )
: EffectNode( format )
{
}

EffectXAudioFilter::~EffectXAudioFilter()
{

}

void EffectXAudioFilter::initialize()
{
	XAudioVoice v = getXAudioVoice( shared_from_this() );

	if( v.node->isFilterConnected() )
		throw AudioContextExc( "source voice already has a filter connected." );
	v.node->setFilterConnected();

	mInitialized = true;
	LOG_V << "complete." << endl;
}

void EffectXAudioFilter::uninitialize()
{
	mInitialized = false;
}

void EffectXAudioFilter::getParams( ::XAUDIO2_FILTER_PARAMETERS *params )
{
	if( ! mInitialized )
		throw AudioParamExc( "must be initialized before accessing params" );

	XAudioVoice v = getXAudioVoice( shared_from_this() );

	v.voice->GetFilterParameters( params );
}

void EffectXAudioFilter::setParams( const ::XAUDIO2_FILTER_PARAMETERS &params )
{
	if( ! mInitialized )
		throw AudioParamExc( "must be initialized before accessing params" );

	XAudioVoice v = getXAudioVoice( shared_from_this() );

	HRESULT hr = v.voice->SetFilterParameters( &params );
	CI_ASSERT( hr == S_OK );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - ContextXAudio
// ----------------------------------------------------------------------------------------------------

ContextXAudio::~ContextXAudio()
{
	if( mInitialized )
		uninitialize();
}

ContextRef ContextXAudio::createContext()
{
	return ContextRef( new ContextXAudio() );
}

LineOutNodeRef ContextXAudio::createLineOut( DeviceRef device, const Node::Format &format )
{
	return makeNode( new LineOutXAudio( device, format ) );
}

LineInNodeRef ContextXAudio::createLineIn( DeviceRef device, const Node::Format &format )
{
	return makeNode( new LineInWasapi( device ) );
}

MixerNodeRef ContextXAudio::createMixer( const Node::Format &format )
{
	return MixerNodeRef(); // note: removed because of MixerXAudio's wonkiness, MixerNode should be used instead
}

RootNodeRef ContextXAudio::getRoot()
{
	if( ! mRoot ) {
		// TODO: allow for variable channel / samplerate
		mRoot = createLineOut( Device::getDefaultOutput() );
		DeviceOutputXAudio *outputXAudio = dynamic_cast<DeviceOutputXAudio *>( dynamic_pointer_cast<LineOutXAudio>( mRoot )->getDevice().get() );
		outputXAudio->initialize();
	}
	return mRoot;
}

void ContextXAudio::initialize()
{
	if( mInitialized )
		return;
	CI_ASSERT( mRoot );

	//mSampleRate = mRoot->getSampleRate();
	//mNumFramesPerBlock = mRoot->getNumFramesPerBlock();

	initNode( mRoot );
	initEffects( mRoot->getInputs().front() );

	mInitialized = true;
	LOG_V << "graph initialize complete. output channels: " << getRoot()->getNumChannels() << endl;
}


//void ContextXAudio::initNode( NodeRef node )
//{
//	if( ! node )
//		return;
//
//	setContext( node );
//
//	// recurse through sources
//	for( size_t i = 0; i < node->getInputs().size(); i++ ) {
//		NodeRef source = node->getInputs()[i];
//
//		if( ! source )
//			continue;
//
//		// if source is generic, if it does it needs a SourceXAudio so add one implicitly
//		shared_ptr<SourceVoiceXAudio> sourceVoice;
//
//		if( ! isNodeNativeXAudio( source ) ) {
//			sourceVoice = getSourceVoice( source );
//			if( ! sourceVoice ) {
//				// first check if any child is a native node - if it is, that indicates we need a custom XAPO
//				if( getXAudioNode( source ) )
//					throw AudioContextExc( "Detected generic node after native Xapo, custom Xapo's not implemented." );
//
//				sourceVoice = shared_ptr<SourceVoiceXAudio>( new SourceVoiceXAudio() );
//				node->getInputs()[i] = sourceVoice;
//				sourceVoice->setParent( node );
//				sourceVoice->setInput( source );
//			}
//		}
//
//		initNode( source );
//
//		// initialize source voice after node
//		if( sourceVoice && ! sourceVoice->isInitialized() ) {
//			sourceVoice->setNumChannels( source->getNumChannels() );
//			setContext( sourceVoice );
//			sourceVoice->setFilterEnabled(); // TODO: detect if there is an effect upstream before enabling filters
//			sourceVoice->initialize();
//		}
//	}
//
//	for( size_t bus = 0; bus < node->getInputs().size(); bus++ ) {
//		NodeRef& sourceNode = node->getInputs()[bus];
//		if( ! sourceNode )
//			continue;
//
//		if( ! node->supportsSourceNumChannels( sourceNode->getNumChannels() ) ) {
//			CI_ASSERT( 0 && "ConverterXAudio not yet implemented" );
//
//			// TODO: insert Converter based on xaudio2 submix voice
//		}
//	}
//
//	node->initialize();
//}

void ContextXAudio::initNode( NodeRef node )
{
	if( ! node )
		return;

	// recurse through inputs
	for( NodeRef& inputNode : node->getInputs() )
		initNode( inputNode );

	node->initialize();
}

void ContextXAudio::connectionsDidChange( const NodeRef &node )
{
	// recurse through inputs
	for( size_t i = 0; i < node->getInputs().size(); i++ ) {
		NodeRef input = node->getInputs()[i];
		if( ! input )
			continue;

		// if input is generic, it needs a SourceXAudio so add one implicitly
		if( ! isNodeNativeXAudio( input ) ) {
			shared_ptr<SourceVoiceXAudio> sourceVoice = getSourceVoice( input );
			if( ! sourceVoice ) {
				// first check if any child is a native node - if it is, that indicates we need a custom XAPO. TODO: this logic is outdated now
				if( getXAudioNode( input ) ) {
					//throw AudioContextExc( "Detected generic node after native Xapo, custom Xapo's not implemented." );
					LOG_E << "Detected generic node after native Xapo, custom Xapo's not implemented." << endl;
					continue;
				}

				LOG_V << "implicit connection: " << input->getTag() << " -> SourceVoiceXAudio -> " << node->getTag() << endl;

				sourceVoice = makeNode( new SourceVoiceXAudio() );
				sourceVoice->setOutput( node );
				sourceVoice->setInput( input );

				sourceVoice->setNumChannels( input->getNumChannels() );
				sourceVoice->setFilterEnabled(); // TODO: detect if there is an effect upstream before enabling filters
				sourceVoice->initialize();

				node->setInput( sourceVoice, i );
			}
		}
	}
}

void ContextXAudio::uninitialize()
{
	if( ! mInitialized )
		return;

	stop();
	uninitNode( mRoot );
	mInitialized = false;
}

void ContextXAudio::uninitNode( NodeRef node )
{
	if( ! node )
		return;
	for( auto &source : node->getInputs() )
		uninitNode( source );

	node->uninitialize();
}

// It appears IXAudio2Voice::SetEffectChain should only be called once - i.e. setting the chain
// with length 1 and then again setting it with length 2 causes the engine to go down when the 
// dsp loop starts.  To overcome this, initEffects recursively looks for all XAudioNode's that 
// have effects attatched to them (during the first graph traversal) and sets the chain just once.
void ContextXAudio::initEffects( NodeRef node )
{
	if( ! node )
		return;
	for( NodeRef& node : node->getInputs() )
		initEffects( node );

	auto nodeXAudio = dynamic_pointer_cast<NodeXAudio>( node ); // TODO: replace with static method check
	if( nodeXAudio && ! nodeXAudio->getEffectsDescriptors().empty() ) {
		XAudioVoice v = nodeXAudio->getXAudioVoice( node );
		CI_ASSERT( v.node == nodeXAudio.get() );
		::XAUDIO2_EFFECT_CHAIN effectsChain;
		effectsChain.EffectCount = nodeXAudio->getEffectsDescriptors().size();
		effectsChain.pEffectDescriptors = nodeXAudio->getEffectsDescriptors().data();

		LOG_V << "SetEffectChain, p: " << (void*)nodeXAudio.get() << ", count: " << effectsChain.EffectCount << endl;
		HRESULT hr = v.voice->SetEffectChain( &effectsChain );
		CI_ASSERT( hr == S_OK );
	}
}

IXAudio2* ContextXAudio::getXAudio()
{
	DeviceOutputXAudio *outputXAudio = dynamic_cast<DeviceOutputXAudio *>( dynamic_pointer_cast<LineOutXAudio>( mRoot )->getDevice().get() );
	return outputXAudio->getXAudio();
}

} } } // namespace cinder::audio2::msw