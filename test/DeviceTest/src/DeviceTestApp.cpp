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

// FIXME: changing buffer size on the fly is still turning off asyn I/O

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace ci::audio2;

class DeviceTestApp : public AppNative {
  public:
	void setup();
	void update();
	void draw();

	void setupUI();
	void processTap( Vec2i pos );
	void keyDown( KeyEvent event );

	void setupDefaultDevices();
	void setupDedicatedDevice();
	void printDeviceDetails();

	void setupSine();
	void setupIOClean();
	void setupNoise();
	void setupIOProcessed();

	ContextRef mContext;
	NodeLineInRef mLineIn;
	NodeLineOutRef mLineOut;
	TapNodeRef mTap;
	NodeGainRef mGain;
	NodeSourceRef mSourceNode;

	vector<TestWidget *> mWidgets;
	VSelector mTestSelector;
	Button mPlayButton;
	TextInput mSamplerateInput, mFramesPerBlockInput;

	Anim<float> mUnderrunFade, mOverrunFade, mClipFade;
	Rectf mUnderrunRect, mOverrunRect, mClipRect;
};

void DeviceTestApp::setup()
{
	mContext = Context::create();

	setupDefaultDevices();
	printDeviceDetails();

	mGain = mContext->makeNode( new NodeGain() );
	mTap = mContext->makeNode( new NodeTap() );

	mGain->connect( mTap )->connect( mContext->getTarget() );

	// TODO: add this as a test control
	//mLineIn->getFormat().setNumChannels( 1 );

	setupSine();

	setupUI();

	printGraph( mContext );

	mLineOut->getDevice()->getSignalParamsDidChange().connect( [this] {	printDeviceDetails(); } );

	LOG_V << "Context samplerate: " << mContext->getSampleRate() << endl;
}

void DeviceTestApp::setupDefaultDevices()
{
	mLineIn = mContext->createLineIn();
	mLineOut = mContext->createLineOut();

	LOG_V << "input == output: " << boolalpha << ( mLineIn->getDevice() == mLineOut->getDevice() ) << dec << endl;

	mContext->setTarget( mLineOut );
}

void DeviceTestApp::printDeviceDetails()
{
	LOG_V << "output device name: " << mLineOut->getDevice()->getName() << endl;
	console() << "\t channels: " << mLineOut->getDevice()->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << mLineOut->getDevice()->getSampleRate() << endl;
	console() << "\t block size: " << mLineOut->getDevice()->getFramesPerBlock() << endl;

	LOG_V << "input device name: " << mLineIn->getDevice()->getName() << endl;
	console() << "\t channels: " << mLineIn->getDevice()->getNumInputChannels() << endl;
	console() << "\t samplerate: " << mLineIn->getDevice()->getSampleRate() << endl;
	console() << "\t block size: " << mLineIn->getDevice()->getFramesPerBlock() << endl;
}

void DeviceTestApp::setupDedicatedDevice()
{
	DeviceRef device = Device::findDeviceByName( "PreSonus FIREPOD (1431)" );
	CI_ASSERT( device );

	mLineIn = mContext->createLineIn( device );
	auto output = mContext->createLineOut( device );
	mContext->setTarget( output );

	LOG_V << "shared device name: " << output->getDevice()->getName() << endl;
	console() << "\t channels: " << output->getDevice()->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << output->getDevice()->getSampleRate() << endl;
	console() << "\t block size: " << output->getDevice()->getFramesPerBlock() << endl;
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
	mTestSelector.mBounds = Rectf( getWindowCenter().x + 100, 0.0f, getWindowWidth(), 160.0f );
#endif

	Rectf textInputBounds( 0.0f, getWindowCenter().y + 40.0f, 200.0f, getWindowCenter().y + 70.0f  );
	mSamplerateInput.mBounds = textInputBounds;
	mSamplerateInput.mTitle = "samplerate";
	mSamplerateInput.setValue( mContext->getSampleRate() );
	mWidgets.push_back( &mSamplerateInput );

	textInputBounds += Vec2f( textInputBounds.getWidth() + 10.0f, 0.0f );
	mFramesPerBlockInput.mBounds = textInputBounds;
	mFramesPerBlockInput.mTitle = "frames per block";
	mFramesPerBlockInput.setValue( mContext->getFramesPerBlock() );
	mWidgets.push_back( &mFramesPerBlockInput );


	Vec2i xrunSize( 80, 26 );
	mUnderrunRect = Rectf( getWindowWidth() - xrunSize.x, getWindowHeight() - xrunSize.y, getWindowWidth(), getWindowHeight() );
	mOverrunRect = mUnderrunRect - Vec2f( xrunSize.x + 10, 0 );
	mClipRect = mOverrunRect - Vec2f( xrunSize.x + 10, 0 );

	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );

	gl::enableAlphaBlending();
}

void DeviceTestApp::processTap( Vec2i pos )
{
	if( mPlayButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );
	else if( mSamplerateInput.hitTest( pos ) ) {
		LOG_V << "mSamplerateInput selected" << endl;
	}
	else if( mFramesPerBlockInput.hitTest( pos ) ) {
		LOG_V << "mFramesPerBlockInput selected" << endl;
	}

	size_t currentIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V << "selected: " << currentTest << endl;

		if( currentTest == "sinewave" ) {
			setupSine();
		}
		if( currentTest == "noise" ) {
			setupNoise();
		}
		if( currentTest == "I/O (clean)" ) {
			setupIOClean();
		}
		if( currentTest == "I/O (processed)" ) {
			setupIOProcessed();
		}

		printDeviceDetails();
	}
}

void DeviceTestApp::keyDown( KeyEvent event )
{
	TextInput *currentSelected = TextInput::getCurrentSelected();
	if( ! currentSelected )
		return;

	if( event.getCode() == KeyEvent::KEY_RETURN ) {
		try {
			if( currentSelected == &mSamplerateInput ) {
				int sr = currentSelected->getValue();
				LOG_V << "updating samplerate from: " << mLineOut->getSampleRate() << " to: " << sr << endl;
				mLineOut->getDevice()->updateParams( Device::Params().sampleRate( sr ) );
			}
			else if( currentSelected == &mFramesPerBlockInput ) {
				int frames = currentSelected->getValue();
				LOG_V << "updating frames per block from: " << mLineOut->getFramesPerBlock() << " to: " << frames << endl;
				mLineOut->getDevice()->updateParams( Device::Params().framesPerBlock( frames ) );
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
}

CINDER_APP_NATIVE( DeviceTestApp, RendererGl )
