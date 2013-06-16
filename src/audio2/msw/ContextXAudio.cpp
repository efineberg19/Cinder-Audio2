#include "audio2/msw/ContextXAudio.h"
#include "audio2/msw/DeviceOutputXAudio.h"
#include "audio2/msw/DeviceInputWasapi.h"

#include "audio2/audio.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"
#include "audio2/Dsp.h"

#include "cinder/Utilities.h"

using namespace std;

namespace audio2 { namespace msw {

static bool isNodeNativeXAudio( NodeRef node ) {
	return dynamic_pointer_cast<NodeXAudio>( node );
}

static shared_ptr<NodeXAudio> getXAudioNode( NodeRef node )
{
	while( node ) {
		auto nodeXAudio = dynamic_pointer_cast<NodeXAudio>( node );
		if( nodeXAudio )
			return nodeXAudio;
		else if( node->getSources().empty() )
			break;

		node = node->getSources().front();
	}
	LOG_V << "No XAudioNode in this tree." << endl;
	return shared_ptr<NodeXAudio>();
}

static shared_ptr<SourceVoiceXAudio> getSourceVoice( NodeRef node )
{
	CI_ASSERT( node );
	while( node ) {
		auto sourceVoice = dynamic_pointer_cast<SourceVoiceXAudio>( node );
		if( sourceVoice )
			return sourceVoice;
		node = node->getParent();
	}
	LOG_V << "No SourceVoiceXAudio in this tree." << endl;
	return shared_ptr<SourceVoiceXAudio>();
}

struct VoiceCallbackImpl : public ::IXAudio2VoiceCallback {

	VoiceCallbackImpl( const function<void()> &callback  ) : mRenderCallback( callback ) {}

	void setSourceVoice( ::IXAudio2SourceVoice *sourceVoice )	{ mSourceVoice = sourceVoice; }

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
	CI_ASSERT( ! node->getSources().empty() );
	NodeRef source = node->getSources().front();

	auto sourceXAudio = dynamic_pointer_cast<NodeXAudio>( source );
	if( sourceXAudio )
		return sourceXAudio->getXAudioVoice( source );

	return getXAudioVoice( source );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - OutputXAudio
// ----------------------------------------------------------------------------------------------------

OutputXAudio::OutputXAudio( DeviceRef device )
: OutputNode( device )
{
	mTag = "OutputAudioUnit";
	mDevice = dynamic_pointer_cast<DeviceOutputXAudio>( device );
	CI_ASSERT( mDevice );

	mFormat.setSampleRate( mDevice->getSampleRate() );
	mFormat.setNumChannels( mDevice->getNumOutputChannels() );
}

void OutputXAudio::initialize()
{
	// Device initialize is handled by the graph because it needs to ensure there is a valid IXAudio instance and mastering voice before anything else is initialized
	//mDevice->initialize();
	mInitialized = true;
}

void OutputXAudio::uninitialize()
{
	mDevice->uninitialize();
	mInitialized = false;
}

void OutputXAudio::start()
{
	mDevice->start();
	LOG_V << "started: " << mDevice->getName() << endl;
}

void OutputXAudio::stop()
{
	mDevice->stop();
	LOG_V << "stopped: " << mDevice->getName() << endl;
}

DeviceRef OutputXAudio::getDevice()
{
	return std::static_pointer_cast<Device>( mDevice );
}

size_t OutputXAudio::getBlockSize() const
{
	// TOOD: this is not yet retrievable from device, if it is necessary, provide value some other way
	return mDevice->getBlockSize();
}

// TODO: check what happens for different samplerates
bool OutputXAudio::supportsSourceFormat( const Node::Format &sourceFormat ) const
{
	return true;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - InputXAudio
// ----------------------------------------------------------------------------------------------------

// TODO: blocksize needs to be exposed.
SourceVoiceXAudio::SourceVoiceXAudio()
{
	mTag = "SourceVoiceXAudio";
	mFormat.setAutoEnabled( true );
	mVoiceCallback = unique_ptr<VoiceCallbackImpl>( new VoiceCallbackImpl( bind( &SourceVoiceXAudio::submitNextBuffer, this ) ) );
}

SourceVoiceXAudio::~SourceVoiceXAudio()
{

}

void SourceVoiceXAudio::initialize()
{
	// TODO: consider whether this should handle higher channel counts, or disallow in graph configure / node format
	CI_ASSERT( mFormat.getNumChannels() <= 2 ); 

	mBuffer = Buffer( mFormat.getNumChannels(), 512 );
	
	size_t numSamples = mBuffer.getSize();

	memset( &mXAudio2Buffer, 0, sizeof( mXAudio2Buffer ) );
	mXAudio2Buffer.AudioBytes = numSamples * sizeof( float );
	if( mFormat.getNumChannels() == 2 ) {
		// setup stereo, XAudio2 requires interleaved samples so point at interleaved buffer
		mBufferInterleaved = Buffer( mBuffer.getNumChannels(), mBuffer.getNumFrames(), Buffer::Format::Interleaved );
		mXAudio2Buffer.pAudioData = reinterpret_cast<BYTE *>( mBufferInterleaved.getData() );
	} else {
		// setup mono
		mXAudio2Buffer.pAudioData = reinterpret_cast<BYTE *>(  mBuffer.getData() );
	}

	auto wfx = msw::interleavedFloatWaveFormat( mFormat.getNumChannels(), mFormat.getSampleRate() );

	UINT32 flags = ( mFilterEnabled ? XAUDIO2_VOICE_USEFILTER : 0 );
	HRESULT hr = mXAudio->CreateSourceVoice( &mSourceVoice, wfx.get(), flags, XAUDIO2_DEFAULT_FREQ_RATIO, mVoiceCallback.get()  );
	CI_ASSERT( hr == S_OK );
	mVoiceCallback->setSourceVoice( mSourceVoice );

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
	mIsRunning = true;
	mSourceVoice->Start();
	submitNextBuffer();

	LOG_V << "started." << endl;
}

void SourceVoiceXAudio::stop()
{
	CI_ASSERT( mSourceVoice );
	mIsRunning = false;
	mSourceVoice->Stop();
	LOG_V << "stopped." << endl;
}

void SourceVoiceXAudio::submitNextBuffer()
{
	CI_ASSERT( mSourceVoice );

	mBuffer.zero();
	renderNode( mSources[0] );

	if( mFormat.getNumChannels() == 2 )
		interleaveStereoBuffer( &mBuffer, &mBufferInterleaved );

	HRESULT hr = mSourceVoice->SubmitSourceBuffer( &mXAudio2Buffer );
	CI_ASSERT( hr == S_OK );
}

void SourceVoiceXAudio::renderNode( NodeRef node )
{
	if( ! node->getSources().empty() )
		renderNode( node->getSources()[0] );

	if( node->isEnabled() )
		node->process( &mBuffer );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - EffectXAudio
// ----------------------------------------------------------------------------------------------------

// TODO: cover the 2 built-ins too, included via xaudio2fx.h
EffectXAudioXapo::EffectXAudioXapo( XapoType type )
: mType( type )
{
	mTag = "EffectXAudioXapo";

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
	effectDesc.OutputChannels = mFormat.getNumChannels();

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

EffectXAudioFilter::EffectXAudioFilter()
{
	mTag = "EffectXAudioFilter";

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
// MARK: - MixerXAudio
// ----------------------------------------------------------------------------------------------------

MixerXAudio::MixerXAudio()
{
	mTag = "MixerXAudio";
	mFormat.setWantsDefaultFormatFromParent();
}

MixerXAudio::~MixerXAudio()
{		
}

void MixerXAudio::initialize()
{
	HRESULT hr = mXAudio->CreateSubmixVoice( &mSubmixVoice, mFormat.getNumChannels(), mFormat.getSampleRate());

	::XAUDIO2_SEND_DESCRIPTOR sendDesc = { 0, mSubmixVoice };
	::XAUDIO2_VOICE_SENDS sendList = { 1, &sendDesc };

	// find source voices and set this node's submix voice to be their output
	// graph should have already inserted a native source voice on this end of the mixer if needed.
	// TODO:: test with generic effects
	for( NodeRef node : mSources ) {
		if( ! node )
			continue;

		auto nodeXAudio = getXAudioNode( node );
		XAudioVoice v = nodeXAudio->getXAudioVoice( node );
		v.voice->SetOutputVoices( &sendList );
	}

	mInitialized = true;
	LOG_V << "initialize complete. " << endl;
}

void MixerXAudio::uninitialize()
{
	// TODO: methinks this should be done at destruction, not un-init
	if( mSubmixVoice ) {
		LOG_V << "about to destroy submix voice: " << hex << mSubmixVoice << dec << endl;
		mSubmixVoice->DestroyVoice();
	}
	mInitialized = false;
}

size_t MixerXAudio::getNumBusses()
{
	size_t result = 0;
	for( NodeRef node : mSources ) {
		if( node )
			result++;
	}
	return result;
}

void MixerXAudio::setNumBusses( size_t count )
{
	// TODO: set what to do here, if anything. probably should be removed.
}

void MixerXAudio::setMaxNumBusses( size_t count )
{
	size_t numActive = getNumBusses();
	if( count < numActive )
		throw AudioExc( string( "don't know how to resize max num busses to " ) + ci::toString( count ) + "when there are " + ci::toString( numActive ) + "active busses." );

	mMaxNumBusses = count;
}

// TODO: test the way getSourceVoice is now implemented in BasicTest

bool MixerXAudio::isBusEnabled( size_t bus )
{
	checkBusIsValid( bus );

	NodeRef node = mSources[bus];
	auto sourceVoice = getSourceVoice( node );

	return sourceVoice->isRunning();
}

void MixerXAudio::setBusEnabled( size_t bus, bool enabled )
{
	checkBusIsValid( bus );

	NodeRef node = mSources[bus];
	auto sourceVoice = getSourceVoice( node );

	if( enabled )
		sourceVoice->stop();
	else
		sourceVoice->start();
}

void MixerXAudio::setBusVolume( size_t bus, float volume )
{
	checkBusIsValid( bus );
	
	NodeRef node = mSources[bus];
	auto nodeXAudio = getXAudioNode( node );
	::IXAudio2Voice *voice = nodeXAudio->getXAudioVoice( node ).voice;

	HRESULT hr = voice->SetVolume( volume );
	CI_ASSERT( hr == S_OK );
}

float MixerXAudio::getBusVolume( size_t bus )
{
	checkBusIsValid( bus );

	NodeRef node = mSources[bus];
	auto nodeXAudio = getXAudioNode( node );
	::IXAudio2Voice *voice = nodeXAudio->getXAudioVoice( node ).voice;

	float volume;
	voice->GetVolume( &volume );
	return volume;
}

// panning explained here: http://msdn.microsoft.com/en-us/library/windows/desktop/hh405043(v=vs.85).aspx
// TODO: panning should be done on an equal power scale, this is linear
void MixerXAudio::setBusPan( size_t bus, float pan )
{
	checkBusIsValid( bus );

	size_t numChannels = mFormat.getNumChannels(); 
	if( numChannels == 1 )
		return; // mono is no-op
	if( numChannels > 2 )
		throw AudioParamExc( string( "Don't know how to pan " ) + ci::toString( numChannels ) + " channels" );

	float left = 0.5f - pan / 2.0f;
	float right = 0.5f + pan / 2.0f; 

	vector<float> outputMatrix( 4 );
	outputMatrix[0] = left;
	outputMatrix[1] = left;
	outputMatrix[2] = right;
	outputMatrix[3] = right;

	NodeRef node = mSources[bus];
	auto nodeXAudio = getXAudioNode( node );
	::IXAudio2Voice *voice = nodeXAudio->getXAudioVoice( node ).voice;
	HRESULT hr = voice->SetOutputMatrix( nullptr, node->getFormat().getNumChannels(), numChannels, outputMatrix.data() );
	CI_ASSERT( hr == S_OK );
}

float MixerXAudio::getBusPan( size_t bus )
{
	checkBusIsValid( bus );
	// TODO: implement
	return 0.0f;
}

void MixerXAudio::checkBusIsValid( size_t bus )
{
	if( bus >= getMaxNumBusses() )
		throw AudioParamExc( "Bus index out of range: " + bus );
	if( ! mSources[bus] )
		throw AudioParamExc( "There is no node at bus index: " + bus );
}

// TODO: check what happens for different samplerates
bool MixerXAudio::supportsSourceFormat( const Node::Format &sourceFormat ) const
{
	return true;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - ConverterXAudio
// ----------------------------------------------------------------------------------------------------

/*
ConverterXAudio::ConverterXAudio( NodeRef source, NodeRef dest, size_t outputBlockSize )
{
	mTag = "ConverterXAudio";
	mFormat = dest->getFormat();
	mSourceFormat = source->getFormat();
	mSources.resize( 1 );

	mRenderContext.currentNode = this;
	mRenderContext.buffer.resize( mSourceFormat.getNumChannels() );
	for( auto& channel : mRenderContext.buffer )
		channel.resize( outputBlockSize );
}

ConverterXAudio::~ConverterXAudio()
{
}

void ConverterXAudio::initialize()
{
	AudioComponentDescription comp{ 0 };
	comp.componentType = kAudioUnitType_FormatConverter;
	comp.componentSubType = kAudioUnitSubType_AUConverter;
	comp.componentManufacturer = kAudioUnitManufacturer_Apple;

	cocoa::findAndCreateAudioComponent( comp, &mAudioUnit );

	::AudioStreamBasicDescription inputAsbd = cocoa::nonInterleavedFloatABSD( mSourceFormat.getNumChannels(), mSourceFormat.getSampleRate() );
	::AudioStreamBasicDescription outputAsbd = cocoa::nonInterleavedFloatABSD( mFormat.getNumChannels(), mFormat.getSampleRate() );

	LOG_V << "input ASBD:" << endl;
	cocoa::printASBD( inputAsbd );
	LOG_V << "output ASBD:" << endl;
	cocoa::printASBD( outputAsbd );


	OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &outputAsbd, sizeof( outputAsbd ) );
	CI_ASSERT( status == noErr );

	status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &inputAsbd, sizeof( inputAsbd ) );
	CI_ASSERT( status == noErr );

	if( mSourceFormat.getNumChannels() == 1 && mFormat.getNumChannels() == 2 ) {
		// map mono source to stereo out
		UInt32 channelMap[2] = { 0, 0 };
		status = ::AudioUnitSetProperty( mAudioUnit, kAudioOutputUnitProperty_ChannelMap, kAudioUnitScope_Output, 0, &channelMap, sizeof( channelMap ) );
		CI_ASSERT( status == noErr );
	}

	status = ::AudioUnitInitialize( mAudioUnit );
	CI_ASSERT( status == noErr );

	LOG_V << "initialize complete. " << endl;
}

void ConverterXAudio::uninitialize()
{
	OSStatus status = ::AudioUnitUninitialize( mAudioUnit );
	CI_ASSERT( status );
}

*/

// ----------------------------------------------------------------------------------------------------
// MARK: - ContextXAudio
// ----------------------------------------------------------------------------------------------------

ContextXAudio::~ContextXAudio()
{
	if( mInitialized )
		uninitialize();
}

InputNodeRef ContextXAudio::createInput( DeviceRef device )
{
	return InputNodeRef( new InputWasapi( device ) );
}

void ContextXAudio::initialize()
{
	if( mInitialized )
		return;
	CI_ASSERT( mRoot );

	// TODO: what about when outputting to file? Do we still need a device?
	// - probably requires abstracting to RootXAudio - if not a device output, we implicitly have one but it is muted
	DeviceOutputXAudio *outputXAudio = dynamic_cast<DeviceOutputXAudio *>( dynamic_pointer_cast<OutputXAudio>( mRoot )->getDevice().get() );
	outputXAudio->initialize();

	initNode( mRoot );
	initEffects( mRoot->getSources().front() );

	mInitialized = true;
	LOG_V << "graph initialize complete. output channels: " <<outputXAudio->getNumOutputChannels() << endl;
}

void ContextXAudio::initNode( NodeRef node )
{
	setXAudio( node );

	Node::Format& format = node->getFormat();

	// set default params from parent if requested // TODO: switch these checks, and one below as well
	if( ! format.isComplete() && format.wantsDefaultFormatFromParent() ) {
		NodeRef parent = node->getParent();
		while( parent ) {
			if( ! format.getSampleRate() )
				format.setSampleRate( parent->getFormat().getSampleRate() );
			if( ! format.getNumChannels() )
				format.setNumChannels( parent->getFormat().getNumChannels() );
			if( format.isComplete() )
				break;
			parent = parent->getParent();
		}
		CI_ASSERT( format.isComplete() );
	}

	// recurse through sources
	for( size_t i = 0; i < node->getSources().size(); i++ ) {
		NodeRef source = node->getSources()[i];

		if( ! source )
			continue;

		// if source is generic, if it does it needs a SourceXAudio so add one implicitly
		shared_ptr<SourceVoiceXAudio> sourceVoice;

		if( ! isNodeNativeXAudio( source ) ) {
			sourceVoice = getSourceVoice( source );
			if( ! sourceVoice ) {
				// first check if any child is a native node - if it is, that indicates we need a custom XAPO
				// TODO: implement custom Xapo and insert for this. make sure EffectXAudioFilter is handled appropriately as well
				if( getXAudioNode( source ) )
					throw AudioContextExc( "Detected generic node after native Xapo, custom Xapo's not implemented." );

				sourceVoice = make_shared<SourceVoiceXAudio>();
				node->getSources()[i] = sourceVoice;
				sourceVoice->setParent( node );
				sourceVoice->setSource( source );
			}
		}

		initNode( source );

		// initialize source voice after node
		if( sourceVoice && ! sourceVoice->isInitialized() ) {
			sourceVoice->getFormat().setNumChannels( source->getFormat().getNumChannels() );
			sourceVoice->getFormat().setSampleRate( source->getFormat().getSampleRate() );
			setXAudio( sourceVoice );
			sourceVoice->setFilterEnabled(); // TODO: detect if there is an effect upstream before enabling filters
			sourceVoice->initialize();
		}
	}

	// set default params from source
	if( ! format.isComplete() && ! format.wantsDefaultFormatFromParent() ) {
		if( ! format.getSampleRate() )
			format.setSampleRate( node->getSourceFormat().getSampleRate() );
		if( ! format.getNumChannels() )
			format.setNumChannels( node->getSourceFormat().getNumChannels() );
	}

	CI_ASSERT( format.isComplete() );

	for( size_t bus = 0; bus < node->getSources().size(); bus++ ) {
		NodeRef& sourceNode = node->getSources()[bus];
		if( ! sourceNode )
			continue;

		if( ! node->supportsSourceFormat( sourceNode->getFormat() ) ) {
			CI_ASSERT( 0 && "ConverterXAudio not yet implemented" );

			bool needsConverter = false;
			if( format.getSampleRate() != sourceNode->getFormat().getSampleRate() )
				//needsConverter = true;
				throw AudioFormatExc( "non-matching samplerates not supported" );
			if( format.getNumChannels() != sourceNode->getFormat().getNumChannels() ) {
				LOG_V << "CHANNEL MISMATCH: " << sourceNode->getFormat().getNumChannels() << " -> " << format.getNumChannels() << endl;
				needsConverter = true;
			}
			if( needsConverter ) {
				//auto converter = make_shared<ConverterAudioUnit>( sourceNode, node, mOutput->getBlockSize() );
				//converter->getSources()[0] = sourceNode;
				//node->getSources()[bus] = converter;
				//converter->setParent( node->getSources()[bus] );
				//converter->initialize();
			}
		}
	}

	node->initialize();
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
	for( auto &source : node->getSources() )
		uninitNode( source );

	node->uninitialize();
}

void ContextXAudio::setXAudio( NodeRef node )
{
	NodeXAudio *nodeXAudio = dynamic_cast<NodeXAudio *>( node.get() );
	if( nodeXAudio ) {
		DeviceOutputXAudio *outputXAudio = dynamic_cast<DeviceOutputXAudio *>( dynamic_pointer_cast<OutputXAudio>( mRoot )->getDevice().get() );
		nodeXAudio->setXAudio( outputXAudio->getXAudio() );
	}
}

// It appears IXAudio2Voice::SetEffectChain should only be called once - i.e. setting the chain
// with length 1 and then again setting it with length 2 causes the engine to go down when the 
// dsp loop starts.  To overcome this, initEffects recursively looks for all XAudioNode's that 
// have effects attatched to them (during the first graph traversal) and sets the chain just once.
void ContextXAudio::initEffects( NodeRef node )
{
	if( ! node )
		return;
	for( NodeRef& node : node->getSources() )
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


}} // namespace audio2::msw