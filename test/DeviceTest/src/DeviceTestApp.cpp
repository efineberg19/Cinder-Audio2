#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Timeline.h"

#include "audio2/audio.h"
#include "audio2/Context.h"
#include "audio2/NodeEffect.h"
#include "audio2/NodeTap.h"
#include "audio2/Dsp.h"
#include "audio2/Debug.h"

#include "Gui.h"

// TODO: finish testing on-the-fly device changes with fireface

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace ci::audio2;

class DeviceTestApp : public AppNative {
  public:
	void prepareSettings( Settings *settings );
	void setup();
	void update();
	void draw();

	void setOutputDevice( const DeviceRef &device );
	void setInputDevice( const DeviceRef &device );
	void printDeviceDetails( const DeviceRef &device );

	void setupSine();
	void setupIOClean();
	void setupNoise();
	void setupIOProcessed();

	void setupUI();
	void processTap( Vec2i pos );
	void processDrag( Vec2i pos );
	void keyDown( KeyEvent event );

	ContextRef mContext;
	NodeLineInRef mLineIn;
	NodeLineOutRef mLineOut;
	NodeTapRef mTap;
	NodeGainRef mGain;
	NodeSourceRef mSourceNode;

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
	mViewYOffset = 0.0f;

	mContext = Context::create();

	setOutputDevice( Device::getDefaultOutput() );
	setInputDevice( Device::getDefaultInput() );

	mLineOut->getDevice()->getSignalParamsDidChange().connect( [this] {	LOG_V << "LineOut params changed:" << endl; printDeviceDetails( mLineOut->getDevice() ); } );

	// TODO: add this as a test control
	//mLineIn->getFormat().setNumChannels( 1 );

	mGain = mContext->makeNode( new NodeGain() );
	mTap = mContext->makeNode( new NodeTap() );

	mGain->setGain( 0.6f );
	mGain->connect( mTap )->connect( mLineOut );

	setupSine();
	printGraph( mContext );

	setupUI();

	LOG_V << "Context samplerate: " << mContext->getSampleRate() << endl;
}

void DeviceTestApp::setOutputDevice( const DeviceRef &device )
{
	NodeSourceRef currentSource = findUpstreamStreamNode<NodeSource>( mGain );
	SaveNodeEnabledState enabled( currentSource );

	mContext->uninitializeAllNodes();

	mLineOut = mContext->createLineOut( device );
	mLineOut->setInput( mTap, 0 );

	mContext->setTarget( mLineOut );

	mContext->initializeAllNodes();

	LOG_V << "LineOut device properties: " << endl;
	printDeviceDetails( device );
}

void DeviceTestApp::setInputDevice( const DeviceRef &device )
{
	NodeRef currentLineInOutput = ( mLineIn ? mLineIn->getOutput() : NodeRef() );
	SaveNodeEnabledState enabled( mLineIn );

	mLineIn = mContext->createLineIn( device );

	if( currentLineInOutput )
		currentLineInOutput->connect( mLineIn, 0 ); // TODO: this assumes line in was connected at bus 0. support detecting if it was connected on a different bus.

	LOG_V << "LineIn device properties: " << endl;
	printDeviceDetails( device );
}

void DeviceTestApp::printDeviceDetails( const DeviceRef &device )
{
	console() << "\t name: " << device->getName() << endl;
	console() << "\t channels: " << device->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << device->getSampleRate() << endl;
	console() << "\t block size: " << device->getFramesPerBlock() << endl;

	if( mLineIn && mLineOut )
		console() << "input == output: " << boolalpha << ( mLineIn->getDevice() == mLineOut->getDevice() ) << dec << endl;
}

void DeviceTestApp::setupSine()
{
	auto sineGen = mContext->makeNode( new NodeGen<SineGen>() );
	sineGen->getGen().setFreq( 440.0f );
	sineGen->getGen().setAmp( 0.5f );
	mSourceNode = sineGen;

	mSourceNode->connect( mGain, 0 );
	mSourceNode->start();
}

void DeviceTestApp::setupNoise()
{
	auto noiseGen = mContext->makeNode( new NodeGen<NoiseGen>() );
	noiseGen->getGen().setAmp( 0.5f );
	mSourceNode = noiseGen;

	mSourceNode->connect( mGain, 0 );
	mSourceNode->start();
}

void DeviceTestApp::setupIOClean()
{
	mLineIn->connect( mGain, 0 );
	mLineIn->start();
}

void DeviceTestApp::setupIOProcessed()
{
	auto ringMod = mContext->makeNode( new RingMod() );

	mLineIn->connect( ringMod )->connect( mGain, 0 );
	mLineIn->start();
}

void DeviceTestApp::setupUI()
{
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
	mGainSlider.set( mGain->getGain() );
	mWidgets.push_back( &mGainSlider );

	mOutputSelector.mTitle = "Output Devices";
	mOutputSelector.mBounds = Rectf( mTestSelector.mBounds.x1, getWindowCenter().y + 40.0f, getWindowWidth(), getWindowHeight() );
	for( const auto &dev : Device::getOutputDevices() ) {
		if( dev == mLineOut->getDevice() )
			mOutputSelector.mCurrentSectionIndex = mOutputSelector.mSegments.size();
		mOutputSelector.mSegments.push_back( dev->getName() );
	}
	mWidgets.push_back( &mOutputSelector );

	mInputSelector.mTitle = "Input Devices";
	mInputSelector.mBounds = mOutputSelector.mBounds - Vec2f( mOutputSelector.mBounds.getWidth() + 10.0f, 0.0f );
	for( const auto &dev : Device::getInputDevices() ) {
		if( dev == mLineIn->getDevice() )
		mInputSelector.mCurrentSectionIndex = mInputSelector.mSegments.size();
		mInputSelector.mSegments.push_back( dev->getName() );
	}
	mWidgets.push_back( &mInputSelector );

	Rectf textInputBounds( 0.0f, getWindowCenter().y + 40.0f, 200.0f, getWindowCenter().y + 70.0f  );
	mSamplerateInput.mBounds = textInputBounds;
	mSamplerateInput.mTitle = "samplerate";
	mSamplerateInput.setValue( mContext->getSampleRate() );
	mWidgets.push_back( &mSamplerateInput );

	textInputBounds += Vec2f( 0.0f, textInputBounds.getHeight() + 24.0f );
	mFramesPerBlockInput.mBounds = textInputBounds;
	mFramesPerBlockInput.mTitle = "frames per block";
	mFramesPerBlockInput.setValue( mContext->getFramesPerBlock() );
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

	gl::enableAlphaBlending();
}

void DeviceTestApp::processDrag( Vec2i pos )
{
	if( mGainSlider.hitTest( pos ) )
		mGain->setGain( mGainSlider.mValueScaled );
}

void DeviceTestApp::processTap( Vec2i pos )
{
	string keyboardString;
	if( mPlayButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );
	else if( mSamplerateInput.hitTest( pos ) ) {
		LOG_V << "mSamplerateInput selected" << endl;
		keyboardString = mSamplerateInput.mInputString;
	}
	else if( mFramesPerBlockInput.hitTest( pos ) ) {
		LOG_V << "mFramesPerBlockInput selected" << endl;
		keyboardString = mFramesPerBlockInput.mInputString;
	}

#if defined( CINDER_COCOA_TOUCH )
	if( ! keyboardString.empty() ) {
		showKeyboard();

		// TODO: setKeyboardString _has_ to be called after show on first go, since it is created there.
		//		- can be overcome by adding lazy initializing keyboard the first time it's accessor is used
		//		- can also possible kill 2 birds with one stone here: if keyboard property is public, advanced users can manipulate it's appearance via that getter.
		setKeyboardString( keyboardString );
		timeline().apply( &mViewYOffset, -100.0f, 0.4f );
	}
#endif

	size_t currentTestIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentTestIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V << "selected: " << currentTest << endl;

		if( currentTest == "sinewave" )
			setupSine();
		if( currentTest == "noise" )
			setupNoise();
		if( currentTest == "I/O (clean)" )
			setupIOClean();
		if( currentTest == "I/O (processed)" )
			setupIOProcessed();

		printGraph( mContext );
		return;
	}

	size_t currentOutputIndex = mOutputSelector.mCurrentSectionIndex;
	if( mOutputSelector.hitTest( pos ) && currentOutputIndex != mOutputSelector.mCurrentSectionIndex ) {
		DeviceRef dev = Device::findDeviceByName( mOutputSelector.mSegments[mOutputSelector.mCurrentSectionIndex] );
		LOG_V << "selected device named: " << dev->getName() << ", key: " << dev->getKey() << endl;

		setOutputDevice( dev );
	}

}

void DeviceTestApp::keyDown( KeyEvent event )
{
	TextInput *currentSelected = TextInput::getCurrentSelected();
	if( ! currentSelected )
		return;

	if( event.getCode() == KeyEvent::KEY_RETURN ) {
#if defined( CINDER_COCOA_TOUCH )
		hideKeyboard();
		timeline().apply( &mViewYOffset, 0.0f, 0.4f );
#endif

		try {
			if( currentSelected == &mSamplerateInput ) {
				int sr = currentSelected->getValue();
				LOG_V << "updating samplerate from: " << mLineOut->getSampleRate() << " to: " << sr << endl;
				mLineOut->getDevice()->updateFormat( Device::Format().sampleRate( sr ) );
			}
			else if( currentSelected == &mFramesPerBlockInput ) {
				int frames = currentSelected->getValue();
				LOG_V << "updating frames per block from: " << mLineOut->getFramesPerBlock() << " to: " << frames << endl;
				mLineOut->getDevice()->updateFormat( Device::Format().framesPerBlock( frames ) );
			}
			else
				LOG_V << "unhandled return for string: " << currentSelected->mInputString << endl;
		}
		catch( AudioDeviceExc &exc ) {
			LOG_E << "AudioDeviceExc caught, what: " << exc.what() << endl;
			mSamplerateInput.setValue( mContext->getSampleRate() );
			mFramesPerBlockInput.setValue( mContext->getFramesPerBlock() );
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

	if( mTap && mTap->isInitialized() ) {
		const audio2::Buffer &buffer = mTap->getBuffer();

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
		float volume = mTap->getVolume();
		Rectf volumeRect( padding, getWindowHeight() - padding - volumeMeterHeight, padding + volume * ( getWindowWidth() - padding ), getWindowHeight() - padding );
		gl::drawSolidRect( volumeRect );
	}

	drawWidgets( mWidgets );

	if( mUnderrunFade > 0.0001 ) {
		gl::color( ColorA( 1.0f, 0.5f, 0.0f, mUnderrunFade ) );
		gl::drawSolidRect( mUnderrunRect );
		gl::drawStringCentered( "underrun", mUnderrunRect.getCenter(), Color::black() );
	}
	if( mOverrunFade > 0.0001 ) {
		gl::color( ColorA( 1.0f, 0.5f, 0.0f, mOverrunFade ) );
		gl::drawSolidRect( mOverrunRect );
		gl::drawStringCentered( "overrun", mOverrunRect.getCenter(), Color::black() );
	}
	if( mClipFade > 0.0001 ) {
		gl::color( ColorA( 1.0f, 0.1f, 0.0f, mClipFade ) );
		gl::drawSolidRect( mClipRect );
		gl::drawStringCentered( "clip", mClipRect.getCenter(), Color::black() );
	}

	gl::popMatrices();
}

CINDER_APP_NATIVE( DeviceTestApp, RendererGl )
