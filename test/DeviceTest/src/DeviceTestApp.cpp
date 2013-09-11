#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Timeline.h"

#include "audio2/Context.h"
#include "audio2/NodeSource.h"
#include "audio2/EffectNode.h"
#include "audio2/TapNode.h"
#include "audio2/Dsp.h"
#include "audio2/Debug.h"

#include "Gui.h"

// TODO NEXT: TextInput widget for samplerate / frames-per-block


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

	void setupDefaultDevices();
	void setupDedicatedDevice();

	void setupOscillator();
	void setupIOClean();
	void setupNoise();
	void setupIOProcessed();

	ContextRef mContext;
	LineInNodeRef mLineIn;
	LineOutNodeRef mLineOut;
	TapNodeRef mTap;
	GainNodeRef mGain;
	NodeSourceRef mSourceNode;

	VSelector mTestSelector;
	Button mPlayButton;

	Anim<float> mUnderrunFade, mOverrunFade;
	Rectf mUnderrunRect, mOverrunRect;
};

void DeviceTestApp::setup()
{
	mContext = Context::create();

	setupDefaultDevices();
	//setupDedicatedDevice();

	mGain = mContext->makeNode( new GainNode() );
	mTap = mContext->makeNode( new TapNode() );

	mGain->connect( mTap )->connect( mContext->getTarget() );

	// TODO: add this as a test control
	//mLineIn->getFormat().setNumChannels( 1 );

	setupOscillator();

	setupUI();

	printGraph( mContext );

	LOG_V << "Context samplerate: " << mContext->getSampleRate() << endl;
}

void DeviceTestApp::setupDefaultDevices()
{
	mLineIn = mContext->createLineIn();

	Device::getDefaultOutput()->setSampleRate( 48000 );
	Device::getDefaultOutput()->setFramesPerBlock( 32 );

	mLineOut = mContext->createLineOut();

	LOG_V << "input device name: " << mLineIn->getDevice()->getName() << endl;
	console() << "\t channels: " << mLineIn->getDevice()->getNumInputChannels() << endl;
	console() << "\t samplerate: " << mLineIn->getDevice()->getSampleRate() << endl;
	console() << "\t block size: " << mLineIn->getDevice()->getFramesPerBlock() << endl;

	LOG_V << "output device name: " << mLineOut->getDevice()->getName() << endl;
	console() << "\t channels: " << mLineOut->getDevice()->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << mLineOut->getDevice()->getSampleRate() << endl;
	console() << "\t block size: " << mLineOut->getDevice()->getFramesPerBlock() << endl;

	LOG_V << "input == output: " << boolalpha << ( mLineIn->getDevice() == mLineOut->getDevice() ) << dec << endl;

	mContext->setTarget( mLineOut );
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

void DeviceTestApp::setupOscillator()
{
	auto sineGen = mContext->makeNode( new UGenNode<SineGen>() );
	sineGen->getUGen().setFreq( 440.0f );
	sineGen->getUGen().setAmp( 0.5f );
	mSourceNode = sineGen;

	mSourceNode->connect( mGain, 0 );
	mSourceNode->start();
}

void DeviceTestApp::setupNoise()
{
	auto noiseGen = mContext->makeNode( new UGenNode<NoiseGen>() );
	noiseGen->getUGen().setAmp( 0.5f );
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

	mTestSelector.mSegments.push_back( "oscillator" );
	mTestSelector.mSegments.push_back( "noise" );
	mTestSelector.mSegments.push_back( "I/O (clean)" );
	mTestSelector.mSegments.push_back( "I/O (processed)" );

#if defined( CINDER_COCOA_TOUCH )
	mPlayButton.mBounds = Rectf( 0, 0, 120, 60 );
	mTestSelector.mBounds = Rectf( getWindowWidth() - 190, 0.0f, getWindowWidth(), 160.0f );
#else
	mPlayButton.mBounds = Rectf( 0, 0, 200, 60 );
	mTestSelector.mBounds = Rectf( getWindowCenter().x + 100, 0.0f, getWindowWidth(), 160.0f );
#endif

	Vec2i xrunSize( 80, 26 );
	mUnderrunRect = Rectf( getWindowWidth() - xrunSize.x, getWindowHeight() - xrunSize.y, getWindowWidth(), getWindowHeight() );
	mOverrunRect = mUnderrunRect - Vec2f( xrunSize.x + 10, 0 );

	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );

	gl::enableAlphaBlending();
}

void DeviceTestApp::processTap( Vec2i pos )
{
	if( mPlayButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );

	size_t currentIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V << "selected: " << currentTest << endl;

//		bool enabled = mContext->isEnabled();
//		mContext->stop();

//		mContext->disconnectAllNodes();

		if( currentTest == "oscillator" ) {
			setupOscillator();
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

//		mContext->setEnabled( enabled );
	}
}

void DeviceTestApp::update()
{
	const float xrunFadeTime = 1.3f;

	if( mLineIn->getLastUnderrun() )
		timeline().apply( &mUnderrunFade, 1.0f, 0.0f, xrunFadeTime );
	if( mLineIn->getLastOverrun() )
		timeline().apply( &mOverrunFade, 1.0f, 0.0f, xrunFadeTime );
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

	mPlayButton.draw();
	mTestSelector.draw();

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
}

CINDER_APP_NATIVE( DeviceTestApp, RendererGl )
