#include "audio2/GraphXAudio.h"
#include "audio2/DeviceOutputXAudio.h"
#include "audio2/audio.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

#include "cinder/Utilities.h"

using namespace std;

namespace audio2 {

	struct VoiceCallbackImpl : public ::IXAudio2VoiceCallback {

		VoiceCallbackImpl( const function<void()> &callback  ) : mRenderCallback( callback ) {}

		void setSourceVoice( ::IXAudio2SourceVoice *sourceVoice )	{ mSourceVoice = sourceVoice; }

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
}

void OutputXAudio::uninitialize()
{
	mDevice->uninitialize();
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
SourceXAudio::SourceXAudio()
{
	mSources.resize( 1 );
	mVoiceCallback = unique_ptr<VoiceCallbackImpl>( new VoiceCallbackImpl( bind( &SourceXAudio::submitNextBuffer, this ) ) );
}

SourceXAudio::~SourceXAudio()
{

}

void SourceXAudio::initialize()
{
	mBuffer.resize(  mFormat.getNumChannels() );
	for( auto& channel : mBuffer )
		channel.resize( 512 );

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

	HRESULT hr = mXaudio->CreateSourceVoice( &mSourceVoice, (::WAVEFORMATEX*)&wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, mVoiceCallback.get()  );
	CI_ASSERT( hr == S_OK );
	mVoiceCallback->setSourceVoice( mSourceVoice );

	LOG_V << "complete." << endl;
}

void SourceXAudio::uninitialize()
{
}

// TODO: source voice must be made during initialize() pass, so there is a chance start/stop can be called
// before. Decide on throwing, silently failing, or a something better.
void SourceXAudio::start()
{
	CI_ASSERT( mSourceVoice );
	mSourceVoice->Start();
	submitNextBuffer();

	LOG_V << "started." << endl;
}

void SourceXAudio::stop()
{
	CI_ASSERT( mSourceVoice );
	mSourceVoice->Stop();
	LOG_V << "stopped." << endl;
}

void SourceXAudio::submitNextBuffer()
{
	CI_ASSERT( mSourceVoice );

	mSources[0]->render( &mBuffer );

	::XAUDIO2_BUFFER xaudio2Buffer = { 0 };
	xaudio2Buffer.pAudioData = reinterpret_cast<BYTE *>( mBuffer.data() );
	xaudio2Buffer.AudioBytes = mBuffer.size() * sizeof( float );
	HRESULT hr = mSourceVoice->SubmitSourceBuffer( &xaudio2Buffer );
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

/*
EffectXAudio::EffectXAudio(  UInt32 effectSubType )
: mEffectSubType( effectSubType )
{
	mTag = "EffectXAudio";
}

EffectXAudio::~EffectXAudio()
{
}

void EffectXAudio::initialize()
{
	::AudioComponentDescription comp{ 0 };
	comp.componentType = kAudioUnitType_Effect;
	comp.componentSubType = mEffectSubType;
	comp.componentManufacturer = kAudioUnitManufacturer_Apple;
	cocoa::findAndCreateAudioComponent( comp, &mAudioUnit );

	auto source = mSources.front();
	CI_ASSERT( source );

	::AudioStreamBasicDescription asbd = cocoa::nonInterleavedFloatABSD( mFormat.getNumChannels(), mFormat.getSampleRate() );
	OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &asbd, sizeof( asbd ) );
	CI_ASSERT( status == noErr );
	status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &asbd, sizeof( asbd ) );
	CI_ASSERT( status == noErr );

	status = ::AudioUnitInitialize( mAudioUnit );
	CI_ASSERT( status == noErr );

	LOG_V << "initialize complete. " << endl;
}

void EffectXAudio::uninitialize()
{
	OSStatus status = ::AudioUnitUninitialize( mAudioUnit );
	CI_ASSERT( status );
}

void EffectXAudio::setParameter( ::AudioUnitParameterID param, float val )
{
	OSStatus status = ::AudioUnitSetParameter( mAudioUnit, param, kAudioUnitScope_Global, 0, val, 0 );
	CI_ASSERT( status == noErr );
}

*/
// ----------------------------------------------------------------------------------------------------
// MARK: - MixerXAudio
// ----------------------------------------------------------------------------------------------------

/*
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
#if defined( CINDER_COCOA_TOUCH )
	if( mFormat.getNumChannels() > 2 )
		throw AudioParamExc( "iOS mult-channel mixer is limited to two output channels" );
#endif

	::AudioComponentDescription comp{ 0 };
	comp.componentType = kAudioUnitType_Mixer;
	comp.componentSubType = kAudioUnitSubType_MultiChannelMixer;
	comp.componentManufacturer = kAudioUnitManufacturer_Apple;

	cocoa::findAndCreateAudioComponent( comp, &mAudioUnit );

	::AudioStreamBasicDescription asbd = cocoa::nonInterleavedFloatABSD( mFormat.getNumChannels(), mFormat.getSampleRate() );
	OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &asbd, sizeof( asbd ) );
	CI_ASSERT( status == noErr );

	float outputVolume = 1.0f;
	status = ::AudioUnitSetParameter( mAudioUnit, kMultiChannelMixerParam_Volume, kAudioUnitScope_Output, 0, outputVolume, 0 );
	CI_ASSERT( status == noErr );

	CI_ASSERT( mSources.size() <= getNumBusses() );

	for( UInt32 bus = 0; bus < mSources.size(); bus++ ) {
		if( ! mSources[bus] )
			continue;

		Node::Format& sourceFormat = mSources[bus]->getFormat();
		::AudioStreamBasicDescription busAsbd = cocoa::nonInterleavedFloatABSD( sourceFormat.getNumChannels(), sourceFormat.getSampleRate() );

		status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, bus, &busAsbd, sizeof( busAsbd ) );
		CI_ASSERT( status == noErr );

		float inputVolume = 1.0f;
		status = ::AudioUnitSetParameter( mAudioUnit, kMultiChannelMixerParam_Volume, kAudioUnitScope_Input, bus, inputVolume, 0 );
		CI_ASSERT( status == noErr );
	}

	status = ::AudioUnitInitialize( mAudioUnit );
	CI_ASSERT( status == noErr );

	LOG_V << "initialize complete. " << endl;
}

void MixerXAudio::uninitialize()
{
	OSStatus status = ::AudioUnitUninitialize( mAudioUnit );
	CI_ASSERT( status );
}

size_t MixerXAudio::getNumBusses()
{
	UInt32 busCount;
	UInt32 busCountSize = sizeof( busCount );
	OSStatus status = ::AudioUnitGetProperty( mAudioUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &busCount, &busCountSize );
	CI_ASSERT( status == noErr );

	return static_cast<size_t>( busCount );
}

void MixerXAudio::setNumBusses( size_t count )
{
	UInt32 busCount = static_cast<UInt32>( count );
	OSStatus status = ::AudioUnitSetProperty( mAudioUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &busCount, sizeof( busCount ) );
	CI_ASSERT( status == noErr );
}

bool MixerXAudio::isBusEnabled( size_t bus )
{
	checkBusIsValid( bus );

	::AudioUnitElement busElement = static_cast<::AudioUnitElement>( bus );
	::AudioUnitParameterValue enabledValue;
	OSStatus status = ::AudioUnitGetParameter( mAudioUnit, kMultiChannelMixerParam_Enable, kAudioUnitScope_Input, busElement, &enabledValue );
	CI_ASSERT( status == noErr );

	return static_cast<bool>( enabledValue );
}

void MixerXAudio::setBusEnabled( size_t bus, bool enabled )
{
	checkBusIsValid( bus );

	::AudioUnitElement busElement = static_cast<::AudioUnitElement>( bus );
	::AudioUnitParameterValue enabledValue = static_cast<::AudioUnitParameterValue>( enabled );
	OSStatus status = ::AudioUnitSetParameter( mAudioUnit, kMultiChannelMixerParam_Enable, kAudioUnitScope_Input, busElement, enabledValue, 0 );
	CI_ASSERT( status == noErr );
}

void MixerXAudio::setBusVolume( size_t bus, float volume )
{
	checkBusIsValid( bus );
	audioUnitSetParam( mAudioUnit, kMultiChannelMixerParam_Volume, volume, kAudioUnitScope_Input, bus );
}

float MixerXAudio::getBusVolume( size_t bus )
{
	checkBusIsValid( bus );

	float volume;
	audioUnitGetParam( mAudioUnit, kMultiChannelMixerParam_Volume, volume, kAudioUnitScope_Input, bus );
	return volume;
}

void MixerXAudio::setBusPan( size_t bus, float pan )
{
	checkBusIsValid( bus );
	audioUnitSetParam( mAudioUnit, kMultiChannelMixerParam_Pan, pan, kAudioUnitScope_Input, bus );
}

float MixerXAudio::getBusPan( size_t bus )
{
	checkBusIsValid( bus );

	float pan;
	audioUnitGetParam( mAudioUnit, kMultiChannelMixerParam_Pan, pan, kAudioUnitScope_Input, bus );
	return pan;
}

void MixerXAudio::checkBusIsValid( size_t bus )
{
	if( bus >= getNumBusses() )
		throw AudioParamExc( "Bus number out of range.");
}

*/
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

	DeviceOutputXAudio *outputXAudio = dynamic_cast<DeviceOutputXAudio *>( dynamic_pointer_cast<OutputXAudio>( mOutput )->getDevice().get() );
	outputXAudio->initialize();

	initNode( mOutput );

	//size_t blockSize = mOutput->getBlockSize();
	//mRenderContext.buffer.resize( mOutput->getFormat().getNumChannels() );
	//for( auto& channel : mRenderContext.buffer )
	//	channel.resize( blockSize );
	//mRenderContext.currentNode = mOutput.get();

	//mInitialized = true;
	//LOG_V << "graph initialize complete. output channels: " << mRenderContext.buffer.size() << ", blocksize: " << blockSize << endl;
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

		initNode( source );

		// if source is generic, add implicit SourceXAudio
		// TODO: check all edge cases (such as generic effect later in the chain)
		if( ! dynamic_cast<XAudioNode *>( source.get() ) ) {
			NodeRef sourceVoice = make_shared<SourceXAudio>();
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
		bool needsConverter = false;
		if( format.getSampleRate() != sourceNode->getFormat().getSampleRate() )
#if 0
			needsConverter = true;
#else
			throw AudioFormatExc( "non-matching samplerates not supported" );
#endif
		if( format.getNumChannels() != sourceNode->getFormat().getNumChannels() ) {
			LOG_V << "CHANNEL MISMATCH: " << sourceNode->getFormat().getNumChannels() << " -> " << format.getNumChannels() << endl;
			// TODO: if node is an OutputAudioUnit, or Mixer, they can do the channel mapping and avoid the converter
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