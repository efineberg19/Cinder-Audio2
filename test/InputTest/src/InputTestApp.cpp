#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/Context.h"
#include "audio2/GeneratorNode.h"
#include "audio2/EffectNode.h"
#include "audio2/TapNode.h"
#include "audio2/Dsp.h"
#include "audio2/Debug.h"

#include "Gui.h"

// TODO: test multiple formats for input
// - make sure inputs and outputs with different samplerates somehow works correctly (which was default for my win8 laptop)

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace ci::audio2;

class InputTestApp : public AppNative {
  public:
	void setup();
	void update();
	void draw();

	void setupUI();
	void processTap( Vec2i pos );

	void setupDefaultDevices();
	void setupDedicatedDevice();

	void setupPassThrough();
	void setupInProcessOut();
	void setupInTapOut();
	void setupInTapProcessOut();

	ContextRef mContext;
	LineInNodeRef mLineIn;
	LineOutNodeRef mLineOut;
	TapNodeRef mTap;

	VSelector mTestSelector;
	Button mPlayButton;
};

void InputTestApp::setup()
{
//	LOG_V << "all devices: " << endl;
//	printDevices();

	mContext = Context::create();

	setupDefaultDevices();
	//setupDedicatedDevice();

	mLineIn->setAutoEnabled();

	// TODO: add this as a test control
	//mLineIn->getFormat().setNumChannels( 1 );

	setupInTapOut();
//	setupInTapProcessOut();

	setupUI();

	printGraph( mContext );
}

void InputTestApp::setupDefaultDevices()
{
	mLineIn = mContext->createLineIn();
	mLineOut = mContext->createLineOut();

	LOG_V << "input device name: " << mLineIn->getDevice()->getName() << endl;
	console() << "\t channels: " << mLineIn->getDevice()->getNumInputChannels() << endl;
	console() << "\t samplerate: " << mLineIn->getDevice()->getSampleRate() << endl;
	console() << "\t block size: " << mLineIn->getDevice()->getNumFramesPerBlock() << endl;

	LOG_V << "output device name: " << mLineOut->getDevice()->getName() << endl;
	console() << "\t channels: " << mLineOut->getDevice()->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << mLineOut->getDevice()->getSampleRate() << endl;
	console() << "\t block size: " << mLineOut->getDevice()->getNumFramesPerBlock() << endl;

	LOG_V << "input == output: " << boolalpha << ( mLineIn->getDevice() == mLineOut->getDevice() ) << dec << endl;
}

void InputTestApp::setupDedicatedDevice()
{
	DeviceRef device = Device::findDeviceByName( "PreSonus FIREPOD (1431)" );
	CI_ASSERT( device );

	mLineIn = mContext->createLineIn( device );
	auto output = mContext->createLineOut( device );
	mContext->setRoot( output );

	LOG_V << "shared device name: " << output->getDevice()->getName() << endl;
	console() << "\t channels: " << output->getDevice()->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << output->getDevice()->getSampleRate() << endl;
	console() << "\t block size: " << output->getDevice()->getNumFramesPerBlock() << endl;
}

void InputTestApp::setupPassThrough()
{
	mLineIn->connect( mContext->getRoot() );
}

void InputTestApp::setupInProcessOut()
{
	auto ringMod = mContext->makeNode( new RingMod() );
	mLineIn->connect( ringMod )->connect( mContext->getRoot() );
}

void InputTestApp::setupInTapOut()
{
	mTap = mContext->makeNode( new TapNode() );
	mLineIn->connect( mTap )->connect( mContext->getRoot() );
}

void InputTestApp::setupInTapProcessOut()
{
	mTap = mContext->makeNode( new TapNode() );
	auto ringMod = mContext->makeNode( new RingMod() );
	mLineIn->connect( mTap )->connect( ringMod )->connect( mContext->getRoot() );
}

void InputTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );

	mTestSelector.mSegments.push_back( "pass through" );
	mTestSelector.mSegments.push_back( "in tap out" );
	mTestSelector.mSegments.push_back( "in process out" );
	mTestSelector.mSegments.push_back( "in tap process out" );
	mTestSelector.mCurrentSectionIndex = 1;

#if defined( CINDER_COCOA_TOUCH )
	mPlayButton.mBounds = Rectf( 0, 0, 120, 60 );
	mTestSelector.mBounds = Rectf( getWindowWidth() - 190, 0.0f, getWindowWidth(), 160.0f );
#else
	mPlayButton.mBounds = Rectf( 0, 0, 200, 60 );
	mTestSelector.mBounds = Rectf( getWindowCenter().x + 100, 0.0f, getWindowWidth(), 160.0f );
#endif

	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );

	gl::enableAlphaBlending();
}

void InputTestApp::processTap( Vec2i pos )
{
	if( mPlayButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );

	size_t currentIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V << "selected: " << currentTest << endl;

		bool enabled = mContext->isEnabled();
		mContext->stop();

		mContext->disconnectAllNodes();

		if( currentTest == "pass through" ) {
			setupPassThrough();
		}
		if( currentTest == "in tap out" ) {
			setupInTapOut();
		}
		if( currentTest == "in process out" ) {
			setupInProcessOut();
		}
		if( currentTest == "in tap process out" ) {
			setupInTapProcessOut();
		}

		mContext->setEnabled( enabled );
	}
}


void InputTestApp::update()
{
}

void InputTestApp::draw()
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
}

CINDER_APP_NATIVE( InputTestApp, RendererGl )
