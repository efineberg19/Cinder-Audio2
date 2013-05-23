#include "audio2/GraphXAudio.h"
#include "audio2/DeviceOutputXAudio.h"
#include "audio2/audio.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"
#include "audio2/UGen.h" // buffer interleaving currently lives in here. TODO: if that is to remain, consider renaming file back to dsp.h/cpp

#include "cinder/Utilities.h"

using namespace std;

namespace audio2 {

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

XAudioNode::~XAudioNode()
{
}

XAudioVoice XAudioNode::getXAudioVoice( NodeRef node )
{
	while( node ) {
		auto nodeXAudio = dynamic_pointer_cast<XAudioNode>( node );
		if( nodeXAudio )
			return nodeXAudio->getXAudioVoice( node );
		else {
			CI_ASSERT( ! node->getSources().empty() );
			node = node->getSources().front();
		}
	}
	CI_ASSERT( false && "unreachable" ); // ???: throw?
	return XAudioVoice();
}

shared_ptr<XAudioNode> XAudioNode::getXAudioNode( NodeRef node )
{
	while( node ) {
		auto nodeXAudio = dynamic_pointer_cast<XAudioNode>( node );
		if( nodeXAudio )
			return nodeXAudio;
		else {
			CI_ASSERT( ! node->getSources().empty() );
			node = node->getSources().front();
		}
	}
	CI_ASSERT( false && "unreachable" ); // ???: throw?
	return shared_ptr<XAudioNode>();
}

shared_ptr<SourceVoiceXAudio> XAudioNode::getSourceVoice( NodeRef node )
{
	CI_ASSERT( node );
	while( node ) {
		auto sourceVoice = dynamic_pointer_cast<SourceVoiceXAudio>( node );
		if( sourceVoice )
			return sourceVoice;
		else {
			CI_ASSERT( ! node->getSources().empty() );
			node = node->getSources().front();
		}
	}
	CI_ASSERT( false && "unreachable" ); // ???: throw?
	return shared_ptr<SourceVoiceXAudio>();
}

size_t XAudioNode::addEffect( const XAudioVoice &voice, const ::XAUDIO2_EFFECT_DESCRIPTOR &effectDesc )
{
	size_t index = mEffectsDescriptors.size();

	mEffectsDescriptors.push_back( effectDesc );

	::XAUDIO2_EFFECT_CHAIN effectsChain;
	effectsChain.EffectCount = mEffectsDescriptors.size();
	effectsChain.pEffectDescriptors = mEffectsDescriptors.data();

	HRESULT hr = voice.voice->SetEffectChain( &effectsChain );
	CI_ASSERT( hr == S_OK );

	return index;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - OutputXAudio
// ----------------------------------------------------------------------------------------------------

OutputXAudio::OutputXAudio( DeviceRef device )
: Output( device )
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

// ----------------------------------------------------------------------------------------------------
// MARK: - InputXAudio
// ----------------------------------------------------------------------------------------------------

// TODO: blocksize needs to be exposed.
SourceVoiceXAudio::SourceVoiceXAudio()
{
	mTag = "SourceVoiceXAudio";
	mSources.resize( 1 );
	mVoiceCallback = unique_ptr<VoiceCallbackImpl>( new VoiceCallbackImpl( bind( &SourceVoiceXAudio::submitNextBuffer, this ) ) );
}

SourceVoiceXAudio::~SourceVoiceXAudio()
{

}

void SourceVoiceXAudio::initialize()
{
	mBuffer.resize(  mFormat.getNumChannels() );
	for( auto& channel : mBuffer )
		channel.resize( 512 );

	mBufferDeInterleaved.resize( mBuffer.size() * mBuffer[0].size() );

	memset( &mXAudio2Buffer, 0, sizeof( mXAudio2Buffer ) );
	mXAudio2Buffer.pAudioData = reinterpret_cast<BYTE *>( mBufferDeInterleaved.data() );
	mXAudio2Buffer.AudioBytes = mBufferDeInterleaved.size() * sizeof( float );

	::WAVEFORMATEXTENSIBLE wfx;
	memset(&wfx, 0, sizeof( ::WAVEFORMATEXTENSIBLE ) );

	wfx.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE ;
	wfx.Format.nSamplesPerSec       = mFormat.getSampleRate();
	wfx.Format.nChannels            = mFormat.getNumChannels();
	wfx.Format.wBitsPerSample       = 32;
	wfx.Format.nBlockAlign          = wfx.Format.nChannels * wfx.Format.wBitsPerSample / 8;
	wfx.Format.nAvgBytesPerSec      = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
	wfx.Format.cbSize               = sizeof( ::WAVEFORMATEXTENSIBLE ) - sizeof( ::WAVEFORMATEX );
	wfx.Samples.wValidBitsPerSample = wfx.Format.wBitsPerSample;
	wfx.SubFormat                   = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
	wfx.dwChannelMask				= 0; // this could be a very complicated bit mask of channel order, but 0 means 'first channel is left, second channel is right, etc'

	HRESULT hr = mXAudio->CreateSourceVoice( &mSourceVoice, (::WAVEFORMATEX*)&wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, mVoiceCallback.get()  );
	CI_ASSERT( hr == S_OK );
	mVoiceCallback->setSourceVoice( mSourceVoice );

	mInitialized = true;
	LOG_V << "complete." << endl;
}

void SourceVoiceXAudio::uninitialize()
{
	mInitialized = false;
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

	mSources[0]->render( &mBuffer );

	// xaudio requires de-interleaved samples
	interleaveStereoBuffer( &mBuffer, &mBufferDeInterleaved );

	HRESULT hr = mSourceVoice->SubmitSourceBuffer( &mXAudio2Buffer );
	CI_ASSERT( hr == S_OK );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - InputXAudio
// ----------------------------------------------------------------------------------------------------

/*
InputXAudio::InputXAudio( DeviceRef device )
: Input( device )
{
	mTag = "InputXAudio";
	mRenderBus = DeviceAudioUnit::Bus::Input;

	mDevice = dynamic_pointer_cast<DeviceAudioUnit>( device );
	CI_ASSERT( mDevice );

	mFormat.setSampleRate( mDevice->getSampleRate() );
	mFormat.setNumChannels( 2 );

	CI_ASSERT( ! mDevice->isInputConnected() );
	mDevice->setInputConnected();
}

InputXAudio::~InputXAudio()
{
}

void InputXAudio::initialize()
{
	::AudioUnit audioUnit = getAudioUnit();
	CI_ASSERT( audioUnit );

	::AudioStreamBasicDescription asbd = cocoa::nonInterleavedFloatABSD( mFormat.getNumChannels(), mFormat.getSampleRate() );

	OSStatus status = ::AudioUnitSetProperty( audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, DeviceAudioUnit::Bus::Input, &asbd, sizeof( asbd ) );
	CI_ASSERT( status == noErr );

	if( mDevice->isOutputConnected() ) {
		LOG_V << "Path A. The High Road." << endl;
		// output node is expected to initialize device, since it is pulling all the way to here.
	}
	else {
		LOG_V << "Path B. initiate ringbuffer" << endl;
		mShouldUseGraphRenderCallback = false;

		mRingBuffer = unique_ptr<RingBuffer>( new RingBuffer( mDevice->getBlockSize() * mFormat.getNumChannels() ) );
		mBufferList = cocoa::createNonInterleavedBufferList( mFormat.getNumChannels(), mDevice->getBlockSize() * sizeof( float ) );

		::AURenderCallbackStruct callbackStruct;
		callbackStruct.inputProc = InputXAudio::inputCallback;
		callbackStruct.inputProcRefCon = this;
		status = ::AudioUnitSetProperty( audioUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, DeviceAudioUnit::Bus::Input, &callbackStruct, sizeof( callbackStruct ) );
		CI_ASSERT( status == noErr );

		mDevice->initialize();
	}

	LOG_V << "initialize complete." << endl;
}

void InputXAudio::uninitialize()
{
	mDevice->uninitialize();
}

void InputXAudio::start()
{

	if( ! mDevice->isOutputConnected() ) {
		mDevice->start();
		LOG_V << "started: " << mDevice->getName() << endl;
	}
}

void InputXAudio::stop()
{
	if( ! mDevice->isOutputConnected() ) {
		mDevice->stop();
		LOG_V << "stopped: " << mDevice->getName() << endl;
	}
}

DeviceRef InputXAudio::getDevice()
{
	return std::static_pointer_cast<Device>( mDevice );
}

::AudioUnit InputXAudio::getAudioUnit() const
{
	return mDevice->getComponentInstance();
}

void InputXAudio::render( BufferT *buffer )
{
	CI_ASSERT( mRingBuffer );

	size_t numFrames = buffer->at( 0 ).size();
	for( size_t c = 0; c < buffer->size(); c++ ) {
		size_t count = mRingBuffer->read( &(*buffer)[c] );
		if( count != numFrames )
			LOG_V << " Warning, unexpected read count: " << count << ", expected: " << numFrames << " (c = " << c << ")" << endl;
	}
}

OSStatus InputXAudio::inputCallback( void *context, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList )
{
	InputXAudio *inputNode = static_cast<InputXAudio *>( context );
	CI_ASSERT( inputNode->mRingBuffer );
	
	::AudioBufferList *nodeBufferList = inputNode->mBufferList.get();
	OSStatus status = ::AudioUnitRender( inputNode->getAudioUnit(), flags, timeStamp, DeviceAudioUnit::Bus::Input, numFrames, nodeBufferList );
	CI_ASSERT( status == noErr );

	for( size_t c = 0; c < nodeBufferList->mNumberBuffers; c++ ) {
		float *channel = static_cast<float *>( nodeBufferList->mBuffers[c].mData );
		inputNode->mRingBuffer->write( channel, numFrames );
	}
	return status;
}

*/

// ----------------------------------------------------------------------------------------------------
// MARK: - EffectXAudio
// ----------------------------------------------------------------------------------------------------


EffectXAudio::EffectXAudio( XapoType type )
: mType( type )
{
	mTag = "EffectXAudio";

	::IUnknown *xapo;
	switch( type ) {
		//case XapoType::FXECHO:				::CreateFX( __uuidof( ::FXECHO ), &xapo ); break; // ???: missing in xaudio 2.8?
		case XapoType::FXEQ:				::CreateFX( __uuidof( ::FXEQ ), &xapo ); break;
		case XapoType::FXMasteringLimiter:	::CreateFX( __uuidof( ::FXMasteringLimiter ), &xapo ); break;
		case XapoType::FXReverb:			::CreateFX( __uuidof( ::FXReverb ), &xapo ); break;
	}
	mXapo = msw::makeComUnique( xapo );
}

EffectXAudio::~EffectXAudio()
{
}

void EffectXAudio::initialize()
{
	::XAUDIO2_EFFECT_DESCRIPTOR effectDesc;
	//effectDesc.InitialState = mEnabled = true; // TODO: consider adding enabled param to Effect base class
	effectDesc.InitialState = true;
	effectDesc.pEffect = mXapo.get();
	effectDesc.OutputChannels = mFormat.getNumChannels();

	NodeRef source = mSources[0];
	XAudioVoice v = getXAudioVoice( source );
	mChainIndex = v.parent->addEffect( v, effectDesc );

	mInitialized = true;
	LOG_V << "successfully added self to effects chain. index: " << mChainIndex << endl;
}

void EffectXAudio::uninitialize()
{
	mInitialized = false;
}

void EffectXAudio::getParams( void *params, size_t sizeParams )
{
	if( ! mInitialized )
		throw AudioParamExc( "must be initialized before accessing params" );

	XAudioVoice v = getXAudioVoice( mSources[0] );
	HRESULT hr = v.voice->GetEffectParameters( mChainIndex, params, sizeParams );
	CI_ASSERT( hr == S_OK );
}

void EffectXAudio::setParams( const void *params, size_t sizeParams )
{
	if( ! mInitialized )
		throw AudioParamExc( "must be initialized before accessing params" );

	XAudioVoice v = getXAudioVoice( mSources[0] );
	HRESULT hr = v.voice->SetEffectParameters( mChainIndex, params, sizeParams );
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
		XAudioNode *nodeXAudio = dynamic_cast<XAudioNode *>( node.get() );
		::IXAudio2Voice *sourceVoice = nodeXAudio->getXAudioVoice( node ).voice;
		sourceVoice->SetOutputVoices( &sendList );
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

bool MixerXAudio::isBusEnabled( size_t bus )
{
	checkBusIsValid( bus );

	NodeRef node = mSources[bus];
	auto nodeXAudio = getXAudioNode( node );
	auto sourceVoice = nodeXAudio->getSourceVoice( node );

	return sourceVoice->isRunning();
}

void MixerXAudio::setBusEnabled( size_t bus, bool enabled )
{
	checkBusIsValid( bus );

	NodeRef node = mSources[bus];
	auto nodeXAudio = getXAudioNode( node );
	auto sourceVoice = nodeXAudio->getSourceVoice( node );

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
// TODO: panning should be done logarithmically, this is linear
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

	return 0.0f;
}

void MixerXAudio::checkBusIsValid( size_t bus )
{
	if( bus >= getMaxNumBusses() )
		throw AudioParamExc( "Bus index out of range: " + bus );
	if( ! mSources[bus] )
		throw AudioParamExc( "There is no node at bus index: " + bus );
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
// MARK: - GraphXAudio
// ----------------------------------------------------------------------------------------------------

GraphXAudio::~GraphXAudio()
{
	
}

void GraphXAudio::initialize()
{
	if( mInitialized )
		return;
	CI_ASSERT( mOutput );

	// TODO: what about when outputting to file? Do we still need a device?
	DeviceOutputXAudio *outputXAudio = dynamic_cast<DeviceOutputXAudio *>( dynamic_pointer_cast<OutputXAudio>( mOutput )->getDevice().get() );
	outputXAudio->initialize();

	initNode( mOutput );

	//size_t blockSize = mOutput->getBlockSize();
	//mRenderContext.buffer.resize( mOutput->getFormat().getNumChannels() );
	//for( auto& channel : mRenderContext.buffer )
	//	channel.resize( blockSize );
	//mRenderContext.currentNode = mOutput.get();

	mInitialized = true;
	LOG_V << "graph initialize complete. output channels: " <<outputXAudio->getNumOutputChannels() << endl;
}

void GraphXAudio::initNode( NodeRef node )
{

	setXAudio( node );

	Node::Format& format = node->getFormat();

	// set default params from parent if requested
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

		initNode( source );

		// if source is generic, add implicit SourceXAudio
		// TODO: check all edge cases (such as generic effect later in the chain)
		if( ! dynamic_cast<XAudioNode *>( source.get() ) ) {
			NodeRef sourceVoice = make_shared<SourceVoiceXAudio>();
			node->getSources()[i] = sourceVoice;
			sourceVoice->getSources()[0] = source;
			sourceVoice->getFormat().setNumChannels( source->getFormat().getNumChannels() );
			sourceVoice->getFormat().setSampleRate( source->getFormat().getSampleRate() );
			setXAudio( sourceVoice );
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

		bool needsConverter = false;
		if( format.getSampleRate() != sourceNode->getFormat().getSampleRate() )
#if 0
			needsConverter = true;
#else
			throw AudioFormatExc( "non-matching samplerates not supported" );
#endif
		if( format.getNumChannels() != sourceNode->getFormat().getNumChannels() ) {
			LOG_V << "CHANNEL MISMATCH: " << sourceNode->getFormat().getNumChannels() << " -> " << format.getNumChannels() << endl;
			// TODO: if node is an Output, or Mixer, they can do the channel mapping and avoid the converter
			needsConverter = true;
		}
		if( needsConverter ) {
			CI_ASSERT( 0 && "conversion not yet implemented" );
			//auto converter = make_shared<ConverterAudioUnit>( sourceNode, node, mOutput->getBlockSize() );
			//converter->getSources()[0] = sourceNode;
			//node->getSources()[bus] = converter;
			//converter->setParent( node->getSources()[bus] );
			//converter->initialize();
			//connectRenderCallback( converter, &converter->mRenderContext, true ); // TODO: make sure this doesn't blow away other converters
		}
	}

	node->initialize();
}

void GraphXAudio::uninitialize()
{
	if( ! mInitialized )
		return;

	stop();
	uninitNode( mOutput );
	mInitialized = false;
}

void GraphXAudio::uninitNode( NodeRef node )
{
	if( ! node )
		return;
	for( auto &source : node->getSources() )
		uninitNode( source );

	node->uninitialize();
}

void GraphXAudio::setXAudio( NodeRef node )
{
	XAudioNode *nodeXAudio = dynamic_cast<XAudioNode *>( node.get() );
	if( nodeXAudio ) {
		DeviceOutputXAudio *outputXAudio = dynamic_cast<DeviceOutputXAudio *>( dynamic_pointer_cast<OutputXAudio>( mOutput )->getDevice().get() );
		nodeXAudio->setXAudio( outputXAudio->getXAudio() );
	}
}

} // namespace audio2