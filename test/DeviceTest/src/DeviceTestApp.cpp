#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Timeline.h"


#include "cinder/audio2/Context.h"
#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/Scope.h"
#include "cinder/audio2/dsp/Dsp.h"
#include "cinder/audio2/Exception.h"
#include "cinder/audio2/Debug.h"

#include "../../common/AudioTestGui.h"

// TODO: finish testing on-the-fly device changes with fireface
// TODO: check iOS 6+ interruption handlers via notification
// TODO: add channels controls for i/o

using namespace ci;
using namespace ci::app;
using namespace std;

class DeviceTestApp : public AppNative {
  public:
	void prepareSettings( Settings *settings );
	void setup();
	void update();
	void draw();

	void setOutputDevice( const audio2::DeviceRef &device );
	void setInputDevice( const audio2::DeviceRef &device );
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
	audio2::NodeSourceRef	mSourceNode;

	vector<TestWidget *> mWidgets;
	VSelector mTestSelector, mInputSelector, mOutputSelector;
	Button mPlayButton;
	HSlider mGainSlider;
	TextInput mSamplerateInput, mFramesPerBlockInput;

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
	auto ctx = audio2::Context::master();

	mScope = ctx->makeNode( new audio2::Scope( audio2::Scope::Format().windowSize( 1024 ) ) );
	mGain = ctx->makeNode( new audio2::Gain() );
	mGain->setValue( 0.6f );

	setOutputDevice( audio2::Device::getDefaultOutput() );
	setInputDevice( audio2::Device::getDefaultInput() );

	mLineOut->getDevice()->getSignalParamsDidChange().connect( [this] {	LOG_V( "LineOut params changed:" ); printDeviceDetails( mLineOut->getDevice() ); } );

	mGain->connect( mScope )->connect( mLineOut );

	setupSine();
//	setupIOClean();

	ctx->printGraph();
	setupUI();

	LOG_V( "Context samplerate: " << ctx->getSampleRate() );
}

void DeviceTestApp::setOutputDevice( const audio2::DeviceRef &device )
{
	audio2::NodeSourceRef currentSource = audio2::findFirstUpstreamNode<audio2::NodeSource>( mGain );
	audio2::SaveNodeEnabledState enabled( currentSource );

	auto ctx = audio2::Context::master();

	ctx->uninitializeAllNodes();

	mLineOut = ctx->createLineOut( device );

	// TODO: if this call is moved to after the mScope->connect(), there is a chance that initialization can
	// take place with samplerate / frames-per-block derived from the default NodeTarget (ses default Device)
	// Double check this doesn't effect anyone, if it does then setTarget may need to do more work to update Nodes.
	ctx->setTarget( mLineOut );

	mScope->connect( mLineOut );

	ctx->initializeAllNodes();

	LOG_V( "LineOut device properties: " );
	printDeviceDetails( device );
}

void DeviceTestApp::setInputDevice( const audio2::DeviceRef &device )
{
	audio2::SaveNodeEnabledState enabled( mLineIn );

	if( mLineIn )
		mLineIn->disconnectAllOutputs();

	mLineIn = audio2::Context::master()->createLineIn( device );

	setupTest( mTestSelector.currentSection() );

	LOG_V( "LineIn device properties: " );
	printDeviceDetails( device );
}

void DeviceTestApp::printDeviceDetails( const audio2::DeviceRef &device )
{
	console() << "\t name: " << device->getName() << endl;
	console() << "\t output channels: " << device->getNumOutputChannels() << endl;
	console() << "\t input channels: " << device->getNumInputChannels() << endl;
	console() << "\t samplerate: " << device->getSampleRate() << endl;
	console() << "\t block size: " << device->getFramesPerBlock() << endl;

	bool isSyncIO = mLineIn && mLineOut && ( mLineIn->getDevice() == mLineOut->getDevice() );

	console() << "\t sync IO: " << boolalpha << isSyncIO << dec << endl;
}

void DeviceTestApp::setupSine()
{
	auto sineGen = audio2::Context::master()->makeNode( new audio2::GenSine() );
	sineGen->setFreq( 440.0f );
	mSourceNode = sineGen;

	mSourceNode->connect( mGain );
	mSourceNode->start();
}

void DeviceTestApp::setupNoise()
{
	auto noiseGen = audio2::Context::master()->makeNode( new audio2::GenNoise() );
	mSourceNode = noiseGen;

	mSourceNode->connect( mGain );
	mSourceNode->start();
}

void DeviceTestApp::setupIOClean()
{
	mLineIn->connect( mGain );
	mLineIn->start();
}

void DeviceTestApp::setupIOProcessed()
{
	auto ctx = audio2::Context::master();
	auto mod = ctx->makeNode( new audio2::GenSine( audio2::Node::Format().autoEnable() ) );
	mod->setFreq( 2 );

	auto ringMod = audio2::Context::master()->makeNode( new audio2::Gain );
	ringMod->getParam()->setModulator( mod );

	// FIXME: second time around mLineIn has a dead pointer in its first slot, the connect tries to use it
	mLineIn->connect( ringMod )->connect( mGain );

	mLineIn->start();
}

void DeviceTestApp::setupUI()
{
	mUnderrunFade = mOverrunFade = mClipFade = 0.0f;
	mViewYOffset = 0.0f;

	mPlayButton = Button( true, "stopped", "playing" );
	mWidgets.push_back( &mPlayButton );

	mTestSelector.mSegments.push_back( "sinewave" );
	mTestSelector.mSegments.push_back( "noise" );
	mTestSelector.mSegments.push_back( "I/O (clean)" );
	mTestSelector.mSegments.push_back( "I/O (processed)" );
	mWidgets.push_back( &mTestSelector );

#if defined( CINDER_COCOA_TOUCH )
	mPlayButton.mBounds = Rectf( 0, 0, 120, 60 );
	mTestSelector.mBounds = Rectf( getWindowWidth() - 190, 0.0f, getWindowWidth(), 160.0f );
#else
	mPlayButton.mBounds = Rectf( 0, 0, 200, 60 );
	mTestSelector.mBounds = Rectf( getWindowCenter().x + 110, 0.0f, getWindowWidth(), 160.0f );
#endif

	mGainSlider.mBounds = Rectf( mTestSelector.mBounds.x1, mTestSelector.mBounds.y2 + 10.0f, mTestSelector.mBounds.x2, mTestSelector.mBounds.y2 + 50.0f );
	mGainSlider.mTitle = "Gain";
	mGainSlider.set( mGain->getValue() );
	mWidgets.push_back( &mGainSlider );

	mOutputSelector.mTitle = "Output Devices";
	mOutputSelector.mBounds = Rectf( mTestSelector.mBounds.x1, getWindowCenter().y + 40.0f, getWindowWidth(), getWindowHeight() );
	for( const auto &dev : audio2::Device::getOutputDevices() ) {
		if( dev == mLineOut->getDevice() )
			mOutputSelector.mCurrentSectionIndex = mOutputSelector.mSegments.size();
		mOutputSelector.mSegments.push_back( dev->getName() );
	}
	mWidgets.push_back( &mOutputSelector );

	mInputSelector.mTitle = "Input Devices";
	mInputSelector.mBounds = mOutputSelector.mBounds - Vec2f( mOutputSelector.mBounds.getWidth() + 10.0f, 0.0f );
	for( const auto &dev : audio2::Device::getInputDevices() ) {
		if( dev == mLineIn->getDevice() )
		mInputSelector.mCurrentSectionIndex = mInputSelector.mSegments.size();
		mInputSelector.mSegments.push_back( dev->getName() );
	}
	mWidgets.push_back( &mInputSelector );

	Rectf textInputBounds( 0.0f, getWindowCenter().y + 40.0f, 200.0f, getWindowCenter().y + 70.0f  );
	mSamplerateInput.mBounds = textInputBounds;
	mSamplerateInput.mTitle = "samplerate";
	mSamplerateInput.setValue( audio2::Context::master()->getSampleRate() );
	mWidgets.push_back( &mSamplerateInput );

	textInputBounds += Vec2f( 0.0f, textInputBounds.getHeight() + 24.0f );
	mFramesPerBlockInput.mBounds = textInputBounds;
	mFramesPerBlockInput.mTitle = "frames per block";
	mFramesPerBlockInput.setValue( audio2::Context::master()->getFramesPerBlock() );
	mWidgets.push_back( &mFramesPerBlockInput );


	Vec2f xrunSize( 80.0f, 26.0f );
	mUnderrunRect = Rectf( 0, mPlayButton.mBounds.y2 + 10.0f, xrunSize.x, mPlayButton.mBounds.y2 + xrunSize.y + 10.0f );
	mOverrunRect = mUnderrunRect + Vec2f( xrunSize.x + 10.0f, 0.0f );
	mClipRect = mOverrunRect + Vec2f( xrunSize.x + 10.0f, 0.0f );

	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalMouseDrag().connect( [this] ( MouseEvent &event ) { processDrag( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );
	getWindow()->getSignalTouchesMoved().connect( [this] ( TouchEvent &event ) {
		for( const TouchEvent::Touch &touch : getActiveTouches() )
			processDrag( touch.getPos() );
	} );

#if defined( CINDER_COCOA_TOUCH )
	getSignalKeyboardWillShow().connect( [this] { timeline().apply( &mViewYOffset, -100.0f, 0.3f, EaseInOutCubic() );	} );
	getSignalKeyboardWillHide().connect( [this] { timeline().apply( &mViewYOffset, 0.0f, 0.3f, EaseInOutCubic() ); } );
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
	if( mPlayButton.hitTest( pos ) )
		audio2::Context::master()->setEnabled( ! audio2::Context::master()->isEnabled() );
	else if( mSamplerateInput.hitTest( pos ) ) {
		LOG_V( "mSamplerateInput selected" );
#if defined( CINDER_COCOA_TOUCH )
		showKeyboard( KeyboardOptions().type( KeyboardType::NUMERICAL ).initialString( mSamplerateInput.mInputString ) );
#endif
	}
	else if( mFramesPerBlockInput.hitTest( pos ) ) {
		LOG_V( "mFramesPerBlockInput selected" );
#if defined( CINDER_COCOA_TOUCH )
		showKeyboard( KeyboardOptions().type( KeyboardType::NUMERICAL ).initialString( mFramesPerBlockInput.mInputString ) );
#endif
	}

	size_t currentTestIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentTestIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V( "selected: " << currentTest );

		setupTest( currentTest );
		return;
	}

	size_t currentOutputIndex = mOutputSelector.mCurrentSectionIndex;
	if( mOutputSelector.hitTest( pos ) && currentOutputIndex != mOutputSelector.mCurrentSectionIndex ) {
		auto dev = audio2::Device::findDeviceByName( mOutputSelector.mSegments[mOutputSelector.mCurrentSectionIndex] );
		LOG_V( "selected device named: " << dev->getName() << ", key: " << dev->getKey() );

		setOutputDevice( dev );
	}
}

void DeviceTestApp::setupTest( const string &test )
{
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

	audio2::Context::master()->printGraph();
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
				LOG_V( "updating samplerate from: " << mLineOut->getSampleRate() << " to: " << sr );
				mLineOut->getDevice()->updateFormat( audio2::Device::Format().sampleRate( sr ) );
			}
			else if( currentSelected == &mFramesPerBlockInput ) {
				int frames = currentSelected->getValue();
				LOG_V( "updating frames per block from: " << mLineOut->getFramesPerBlock() << " to: " << frames );
				mLineOut->getDevice()->updateFormat( audio2::Device::Format().framesPerBlock( frames ) );
			}
			else
				LOG_V( "unhandled return for string: " << currentSelected->mInputString );
		}
		catch( audio2::AudioDeviceExc &exc ) {
			LOG_E( "AudioDeviceExc caught, what: " << exc.what() );
			auto ctx = audio2::Context::master();
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
	gl::color( 0.0f, 0.9f, 0.0f );

	gl::pushMatrices();
	gl::translate( 0.0f, mViewYOffset );

	if( mScope && mScope->isInitialized() ) {
		const audio2::Buffer &buffer = mScope->getBuffer();

		float padding = 20.0f;
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

		float volumeMeterHeight = 20.0f;
		float volume = mScope->getVolume();
		Rectf volumeRect( padding, getWindowHeight() - padding - volumeMeterHeight, padding + volume * ( getWindowWidth() - padding ), getWindowHeight() - padding );
		gl::drawSolidRect( volumeRect );
	}

	drawWidgets( mWidgets );

	if( mUnderrunFade > 0.0001f ) {
		gl::color( ColorA( 1.0f, 0.5f, 0.0f, mUnderrunFade ) );
		gl::drawSolidRect( mUnderrunRect );
		gl::drawStringCentered( "underrun", mUnderrunRect.getCenter(), Color::black() );
	}
	if( mOverrunFade > 0.0001f ) {
		gl::color( ColorA( 1.0f, 0.5f, 0.0f, mOverrunFade ) );
		gl::drawSolidRect( mOverrunRect );
		gl::drawStringCentered( "overrun", mOverrunRect.getCenter(), Color::black() );
	}
	if( mClipFade > 0.0001f ) {
		gl::color( ColorA( 1.0f, 0.1f, 0.0f, mClipFade ) );
		gl::drawSolidRect( mClipRect );
		gl::drawStringCentered( "clip", mClipRect.getCenter(), Color::black() );
	}

	gl::popMatrices();
}

CINDER_APP_NATIVE( DeviceTestApp, RendererGl )
