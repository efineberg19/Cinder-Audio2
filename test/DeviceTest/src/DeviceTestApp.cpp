#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Timeline.h"


#include "cinder/audio2/Context.h"
#include "cinder/audio2/Gen.h"
#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/Scope.h"
#include "cinder/audio2/dsp/Dsp.h"
#include "cinder/audio2/Exception.h"
#include "cinder/audio2/Debug.h"

#include "../../common/AudioTestGui.h"

// TODO: check iOS 6+ interruption handlers via notification

using namespace ci;
using namespace ci::app;
using namespace std;

class DeviceTestApp : public AppNative {
  public:
	void prepareSettings( Settings *settings );
	void setup();
	void update();
	void draw();

	void setOutputDevice( const audio2::DeviceRef &device, size_t numChannels = 0 );
	void setInputDevice( const audio2::DeviceRef &device, size_t numChannels = 0 );
	void setupMultiChannelDevice( const string &deviceName );
	void printDeviceDetails( const audio2::DeviceRef &device );

	void setupSine();
	void setupIOClean();
	void setupNoise();
	void setupIOProcessed();
	void setupTest( const string &test );

	void setupUI();
	void processTap( Vec2i pos );
	void processDrag( Vec2i pos );
	void keyDown( KeyEvent event );

	audio2::LineInRef		mLineIn;
	audio2::LineOutRef		mLineOut;
	audio2::ScopeRef		mScope;
	audio2::GainRef			mGain;
	audio2::GenRef			mGen;

	vector<TestWidget *> mWidgets;
	VSelector mTestSelector, mInputSelector, mOutputSelector;
	Button mPlayButton;
	HSlider mGainSlider;
	TextInput mSamplerateInput, mFramesPerBlockInput, mNumInChannelsInput, mNumOutChannelsInput;

	Anim<float> mUnderrunFade, mOverrunFade, mClipFade;
	Anim<float> mViewYOffset; // for iOS keyboard
	Rectf mUnderrunRect, mOverrunRect, mClipRect;
};

void DeviceTestApp::prepareSettings( Settings *settings )
{
	settings->setWindowSize( 800, 600 );
}

void DeviceTestApp::setup()
{
	auto ctx = audio2::master();

	mScope = ctx->makeNode( new audio2::Scope( audio2::Scope::Format().windowSize( 1024 ) ) );
	mGain = ctx->makeNode( new audio2::Gain() );
	mGain->setValue( 0.6f );

	mGain->connect( mScope );

	setOutputDevice( audio2::Device::getDefaultOutput() );
	setInputDevice( audio2::Device::getDefaultInput() );

//	setupMultiChannelDevice( "PreSonus FIREPOD (1431)" );

	setupSine();
//	setupIOClean();
//	setupIOProcessed();

	ctx->printGraph();

	setupUI();

	CI_LOG_V( "Context samplerate: " << ctx->getSampleRate() );
}

void DeviceTestApp::setOutputDevice( const audio2::DeviceRef &device, size_t numChannels )
{
	auto ctx = audio2::master();

	ctx->uninitializeAllNodes();

	audio2::Node::Format format;
	if( numChannels )
		format.channels( numChannels );

	mLineOut = ctx->createLineOut( device, format );

	mLineOut->getDevice()->getSignalParamsDidChange().connect( [this] {	CI_LOG_V( "LineOut params changed:" ); printDeviceDetails( mLineOut->getDevice() ); } );


	// TODO: if this call is moved to after the mScope->connect(), there is a chance that initialization can
	// take place with samplerate / frames-per-block derived from the default NodeOutput (ses default Device)
	// Double check this doesn't effect anyone, if it does then setOutput may need to do more work to update Nodes.
	ctx->setOutput( mLineOut );

	mScope->connect( mLineOut );

	ctx->initializeAllNodes();

	CI_LOG_V( "LineOut device properties: " );
	printDeviceDetails( device );

	// TODO: considering doing this automatically in Context::setOutput, but then also have to worry about initialize()
	// - also may do a ScopedEnable that handles Context as well.
	if( mPlayButton.mEnabled )
		mLineOut->start();
}

void DeviceTestApp::setInputDevice( const audio2::DeviceRef &device, size_t numChannels  )
{
	audio2::ScopedNodeEnabledState enabled( mLineIn );

	if( mLineIn )
		mLineIn->disconnectAllOutputs();

	audio2::Node::Format format;
	if( numChannels )
		format.channels( numChannels );

	mLineIn = audio2::master()->createLineIn( device, format );

	setupTest( mTestSelector.currentSection() );

	CI_LOG_V( "LineIn device properties: " );
	printDeviceDetails( device );
}

void DeviceTestApp::setupMultiChannelDevice( const string &deviceName )
{
	auto dev = audio2::Device::findDeviceByName( deviceName );
	CI_ASSERT( dev );

	setOutputDevice( dev );
	setInputDevice( dev );
}
void DeviceTestApp::printDeviceDetails( const audio2::DeviceRef &device )
{
	console() << "\t name: " << device->getName() << endl;
	console() << "\t output channels: " << device->getNumOutputChannels() << endl;
	console() << "\t input channels: " << device->getNumInputChannels() << endl;
	console() << "\t samplerate: " << device->getSampleRate() << endl;
	console() << "\t block size: " << device->getFramesPerBlock() << endl;

	bool isSyncIO = mLineIn && mLineOut && ( mLineIn->getDevice() == mLineOut->getDevice() && ( mLineIn->getNumChannels() == mLineOut->getNumChannels() ) );

	console() << "\t sync IO: " << boolalpha << isSyncIO << dec << endl;
}

void DeviceTestApp::setupSine()
{
	mGen = audio2::master()->makeNode( new audio2::GenSine() );
	mGen->setFreq( 440 );

	mGen->connect( mGain );
	mGen->start();
}

void DeviceTestApp::setupNoise()
{
	mGen = audio2::master()->makeNode( new audio2::GenNoise() );

	mGen->connect( mGain );
	mGen->start();
}

void DeviceTestApp::setupIOClean()
{
	mLineIn->connect( mGain );
	mLineIn->start();
}

// sub-classed merely so printGraph prints a more descriptive name
struct RingModGain : public audio2::Gain {
};

void DeviceTestApp::setupIOProcessed()
{
	auto ctx = audio2::master();
	auto mod = ctx->makeNode( new audio2::GenSine( audio2::Node::Format().autoEnable() ) );
	mod->setFreq( 200 );

	auto ringMod = audio2::master()->makeNode( new RingModGain );
	ringMod->getParam()->setProcessor( mod );

	mLineIn >> ringMod >> mGain->bus( 0 );

	mLineIn->start();
}

void DeviceTestApp::setupUI()
{
	mUnderrunFade = mOverrunFade = mClipFade = 0;
	mViewYOffset = 0;

	mPlayButton = Button( true, "stopped", "playing" );
	mWidgets.push_back( &mPlayButton );

	mTestSelector.mSegments.push_back( "sinewave" );
	mTestSelector.mSegments.push_back( "noise" );
	mTestSelector.mSegments.push_back( "I/O (clean)" );
	mTestSelector.mSegments.push_back( "I/O (processed)" );
	mWidgets.push_back( &mTestSelector );

#if defined( CINDER_COCOA_TOUCH )
	mPlayButton.mBounds = Rectf( 0, 0, 120, 60 );
	mTestSelector.mBounds = Rectf( getWindowWidth() - 190, 0, getWindowWidth(), 160 );
#else
	mPlayButton.mBounds = Rectf( 0, 0, 200, 60 );
	mTestSelector.mBounds = Rectf( getWindowCenter().x + 110, 0, (float)getWindowWidth(), 160 );
#endif

	mGainSlider.mBounds = Rectf( mTestSelector.mBounds.x1, mTestSelector.mBounds.y2 + 10, mTestSelector.mBounds.x2, mTestSelector.mBounds.y2 + 50 );
	mGainSlider.mTitle = "Gain";
	mGainSlider.set( mGain->getValue() );
	mWidgets.push_back( &mGainSlider );

	mOutputSelector.mTitle = "Output Devices";
	mOutputSelector.mBounds = Rectf( mTestSelector.mBounds.x1, getWindowCenter().y + 40, (float)getWindowWidth(), (float)getWindowHeight() );
	for( const auto &dev : audio2::Device::getOutputDevices() ) {
		if( dev == mLineOut->getDevice() )
			mOutputSelector.mCurrentSectionIndex = mOutputSelector.mSegments.size();
		mOutputSelector.mSegments.push_back( dev->getName() );
	}
	mWidgets.push_back( &mOutputSelector );

	mInputSelector.mTitle = "Input Devices";
	mInputSelector.mBounds = mOutputSelector.mBounds - Vec2f( mOutputSelector.mBounds.getWidth() + 10, 0 );
	for( const auto &dev : audio2::Device::getInputDevices() ) {
		if( dev == mLineIn->getDevice() )
		mInputSelector.mCurrentSectionIndex = mInputSelector.mSegments.size();
		mInputSelector.mSegments.push_back( dev->getName() );
	}
	mWidgets.push_back( &mInputSelector );

	Rectf textInputBounds( 0, getWindowCenter().y + 40, 200, getWindowCenter().y + 70  );
	mSamplerateInput.mBounds = textInputBounds;
	mSamplerateInput.mTitle = "samplerate";
	mSamplerateInput.setValue( audio2::master()->getSampleRate() );
	mWidgets.push_back( &mSamplerateInput );

	textInputBounds += Vec2f( 0, textInputBounds.getHeight() + 24 );
	mFramesPerBlockInput.mBounds = textInputBounds;
	mFramesPerBlockInput.mTitle = "frames per block";
	mFramesPerBlockInput.setValue( audio2::master()->getFramesPerBlock() );
	mWidgets.push_back( &mFramesPerBlockInput );

	textInputBounds += Vec2f( 0, textInputBounds.getHeight() + 24 );
	mNumInChannelsInput.mBounds = textInputBounds;
	mNumInChannelsInput.mTitle = "num inputs";
	mNumInChannelsInput.setValue( mLineIn->getNumChannels() );
	mWidgets.push_back( &mNumInChannelsInput );

	textInputBounds += Vec2f( 0, textInputBounds.getHeight() + 24 );
	mNumOutChannelsInput.mBounds = textInputBounds;
	mNumOutChannelsInput.mTitle = "num outputs";
	mNumOutChannelsInput.setValue( mLineOut->getNumChannels() );
	mWidgets.push_back( &mNumOutChannelsInput );

	Vec2f xrunSize( 80, 26 );
	mUnderrunRect = Rectf( 0, mPlayButton.mBounds.y2 + 10, xrunSize.x, mPlayButton.mBounds.y2 + xrunSize.y + 10 );
	mOverrunRect = mUnderrunRect + Vec2f( xrunSize.x + 10, 0 );
	mClipRect = mOverrunRect + Vec2f( xrunSize.x + 10, 0 );

	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalMouseDrag().connect( [this] ( MouseEvent &event ) { processDrag( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );
	getWindow()->getSignalTouchesMoved().connect( [this] ( TouchEvent &event ) {
		for( const TouchEvent::Touch &touch : getActiveTouches() )
			processDrag( touch.getPos() );
	} );

#if defined( CINDER_COCOA_TOUCH )
	getSignalKeyboardWillShow().connect( [this] { timeline().apply( &mViewYOffset, -100, 0.3f, EaseInOutCubic() );	} );
	getSignalKeyboardWillHide().connect( [this] { timeline().apply( &mViewYOffset, 0, 0.3f, EaseInOutCubic() ); } );
#endif

	gl::enableAlphaBlending();
}

void DeviceTestApp::processDrag( Vec2i pos )
{
	if( mGainSlider.hitTest( pos ) )
		mGain->setValue( mGainSlider.mValueScaled );
}

void DeviceTestApp::processTap( Vec2i pos )
{
//	TextInput *selectedInput = false;
	if( mPlayButton.hitTest( pos ) )
		audio2::master()->setEnabled( ! audio2::master()->isEnabled() );
	else if( mSamplerateInput.hitTest( pos ) ) {
	}
	else if( mFramesPerBlockInput.hitTest( pos ) ) {
	}
	else if( mNumInChannelsInput.hitTest( pos ) ) {
	}
	else if( mNumOutChannelsInput.hitTest( pos ) ) {
	}

#if defined( CINDER_COCOA_TOUCH )
	TextInput *currentSelected = TextInput::getCurrentSelected();
	if( currentSelected )
		showKeyboard( KeyboardOptions().type( KeyboardType::NUMERICAL ).initialString( currentSelected->mInputString ) );
#endif

	size_t currentTestIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentTestIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		CI_LOG_V( "selected: " << currentTest );

		setupTest( currentTest );
		return;
	}

	size_t currentOutputIndex = mOutputSelector.mCurrentSectionIndex;
	if( mOutputSelector.hitTest( pos ) && currentOutputIndex != mOutputSelector.mCurrentSectionIndex ) {
		auto dev = audio2::Device::findDeviceByName( mOutputSelector.mSegments[mOutputSelector.mCurrentSectionIndex] );
		CI_LOG_V( "selected output device named: " << dev->getName() << ", key: " << dev->getKey() );

		setOutputDevice( dev );
		return;
	}

	size_t currentInputIndex = mInputSelector.mCurrentSectionIndex;
	if( mInputSelector.hitTest( pos ) && currentInputIndex != mInputSelector.mCurrentSectionIndex ) {
		auto dev = audio2::Device::findDeviceByName( mInputSelector.mSegments[mInputSelector.mCurrentSectionIndex] );
		CI_LOG_V( "selected input named: " << dev->getName() << ", key: " << dev->getKey() );

		setInputDevice( dev );
	}
}

void DeviceTestApp::setupTest( const string &test )
{
	// FIXME: Switching from 'noise' to 'i/o' on mac is causing a deadlock when initializing LineInAudioUnit.
	//	- it shouldn't have to be stopped, need to check why.
	//  - temp fix: stop / start context around reconfig
	audio2::master()->stop();

	if( test == "sinewave" )
		setupSine();
	else if( test == "noise" )
		setupNoise();
	else if( test == "I/O (clean)" )
		setupIOClean();
	else if( test == "I/O (processed)" )
		setupIOProcessed();
	else
		setupSine();

	if( mPlayButton.mEnabled )
		audio2::master()->start();

	audio2::master()->printGraph();
}

void DeviceTestApp::keyDown( KeyEvent event )
{
	TextInput *currentSelected = TextInput::getCurrentSelected();
	if( ! currentSelected )
		return;

	if( event.getCode() == KeyEvent::KEY_RETURN ) {
#if defined( CINDER_COCOA_TOUCH )
		hideKeyboard();
#endif

		try {
			if( currentSelected == &mSamplerateInput ) {
				int sr = currentSelected->getValue();
				CI_LOG_V( "updating samplerate from: " << mLineOut->getSampleRate() << " to: " << sr );
				mLineOut->getDevice()->updateFormat( audio2::Device::Format().sampleRate( sr ) );
			}
			else if( currentSelected == &mFramesPerBlockInput ) {
				int frames = currentSelected->getValue();
				CI_LOG_V( "updating frames per block from: " << mLineOut->getFramesPerBlock() << " to: " << frames );
				mLineOut->getDevice()->updateFormat( audio2::Device::Format().framesPerBlock( frames ) );
			}
			else if( currentSelected == &mNumInChannelsInput ) {
				int numChannels = currentSelected->getValue();
				CI_LOG_V( "updating nnm input channels from: " << mLineIn->getNumChannels() << " to: " << numChannels );
				setInputDevice( mLineIn->getDevice(), numChannels );
			}
			else if( currentSelected == &mNumOutChannelsInput ) {
				int numChannels = currentSelected->getValue();
				CI_LOG_V( "updating nnm output channels from: " << mLineOut->getNumChannels() << " to: " << numChannels );
				setOutputDevice( mLineOut->getDevice(), numChannels );
			}
			else
				CI_LOG_E( "unhandled return for string: " << currentSelected->mInputString );
		}
		catch( audio2::AudioDeviceExc &exc ) {
			CI_LOG_E( "AudioDeviceExc caught, what: " << exc.what() );
			auto ctx = audio2::master();
			mSamplerateInput.setValue( ctx->getSampleRate() );
			mFramesPerBlockInput.setValue( ctx->getFramesPerBlock() );
			return;
		}
	}
	else {
		if( event.getCode() == KeyEvent::KEY_BACKSPACE )
			currentSelected->processBackspace();
		else {
			currentSelected->processChar( event.getChar() );
		}
	}
}

void DeviceTestApp::update()
{
	const float xrunFadeTime = 1.3f;

	if( mLineIn->getLastUnderrun() )
		timeline().apply( &mUnderrunFade, 1.0f, 0.0f, xrunFadeTime );
	if( mLineIn->getLastOverrun() )
		timeline().apply( &mOverrunFade, 1.0f, 0.0f, xrunFadeTime );
	if( mLineOut->getLastClip() )
		timeline().apply( &mClipFade, 1.0f, 0.0f, xrunFadeTime );
}

void DeviceTestApp::draw()
{
	gl::clear();
	gl::color( 0, 0.9f, 0 );

	gl::pushMatrices();
	gl::translate( 0, mViewYOffset );

	if( mScope && mScope->isEnabled() ) {
		const audio2::Buffer &buffer = mScope->getBuffer();

		float padding = 20;
		float waveHeight = ((float)getWindowHeight() - padding * 3.0f ) / (float)buffer.getNumChannels();

		float yOffset = padding;
		float xScale = (float)getWindowWidth() / (float)buffer.getNumFrames();
		for( size_t ch = 0; ch < buffer.getNumChannels(); ch++ ) {
			PolyLine2f waveform;
			const float *channel = buffer.getChannel( ch );
			for( size_t i = 0; i < buffer.getNumFrames(); i++ ) {
				float x = i * xScale;
				float y = ( channel[i] * 0.5f + 0.5f ) * waveHeight + yOffset;
				waveform.push_back( Vec2f( x, y ) );
			}
			gl::draw( waveform );
			yOffset += waveHeight + padding;
		}

		float volumeMeterHeight = 20;
		float volume = mScope->getVolume();
		Rectf volumeRect( padding, getWindowHeight() - padding - volumeMeterHeight, padding + volume * ( getWindowWidth() - padding ), getWindowHeight() - padding );
		gl::drawSolidRect( volumeRect );
	}

	drawWidgets( mWidgets );

	if( mUnderrunFade > 0.0001f ) {
		gl::color( ColorA( 0.8f, 0.2f, 0, mUnderrunFade ) );
		gl::drawSolidRect( mUnderrunRect );
		gl::drawStringCentered( "underrun", mUnderrunRect.getCenter(), Color::black() );
	}
	if( mOverrunFade > 0.0001f ) {
		gl::color( ColorA( 0.8f, 0.2f, 0, mOverrunFade ) );
		gl::drawSolidRect( mOverrunRect );
		gl::drawStringCentered( "overrun", mOverrunRect.getCenter(), Color::black() );
	}
	if( mClipFade > 0.0001f ) {
		gl::color( ColorA( 0.8f, 0.2f, 0, mClipFade ) );
		gl::drawSolidRect( mClipRect );
		gl::drawStringCentered( "clip", mClipRect.getCenter(), Color::black() );
	}

	gl::popMatrices();
}

CINDER_APP_NATIVE( DeviceTestApp, RendererGl )
