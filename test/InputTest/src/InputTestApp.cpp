#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/Context.h"
#include "audio2/GeneratorNode.h"
#include "audio2/EffectNode.h"
#include "audio2/TapNode.h"
#include "audio2/Dsp.h"
#include "audio2/Debug.h"

#include "Gui.h"

// FIXME: (mac) crash on shutdown while audio was running
// - in ContextAudioUnit::renderCallback(), renderContext->currentNode was null.
// - may mean calling Context::stop() at shutdown is required, but I hope not.

// TODO: test multiple formats for input
// - make sure inputs and outputs with different samplerates somehow works correctly (which was default for my win8 laptop)

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace audio2;

class InputTestApp : public AppNative {
  public:
	void setup();
	void update();
	void draw();

	void initGraph();

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
	TapNodeRef mTap;

	VSelector mTestSelector;
	Button mPlayButton;
};

void InputTestApp::setup()
{
	mContext = Context::instance()->createContext();

	LOG_V << "all devices: " << endl;
	printDevices();

	setupDefaultDevices();
	//setupDedicatedDevice();


	// TODO: add this as a test control
	//mLineIn->getFormat().setNumChannels( 1 );

	setupInTapOut();

	initGraph();
	setupUI();
}

void InputTestApp::setupDefaultDevices()
{
	mLineIn = Context::instance()->createLineIn();
	auto output = Context::instance()->createLineOut();
	mContext->setRoot( output );

	LOG_V << "input device name: " << mLineIn->getDevice()->getName() << endl;
	console() << "\t channels: " << mLineIn->getDevice()->getNumInputChannels() << endl;
	console() << "\t samplerate: " << mLineIn->getDevice()->getSampleRate() << endl;
	console() << "\t block size: " << mLineIn->getDevice()->getNumFramesPerBlock() << endl;

	LOG_V << "output device name: " << output->getDevice()->getName() << endl;
	console() << "\t channels: " << output->getDevice()->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << output->getDevice()->getSampleRate() << endl;
	console() << "\t block size: " << output->getDevice()->getNumFramesPerBlock() << endl;

	LOG_V << "input == output: " << boolalpha << ( mLineIn->getDevice() == output->getDevice() ) << dec << endl;
}

void InputTestApp::setupDedicatedDevice()
{
	DeviceRef device = Device::findDeviceByName( "PreSonus FIREPOD (1431)" );
	CI_ASSERT( device );

	mLineIn = Context::instance()->createLineIn( device );
	auto output = Context::instance()->createLineOut( device );
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
	auto ringMod = make_shared<RingMod>();
	mLineIn->connect( ringMod )->connect( mContext->getRoot() );
}

void InputTestApp::setupInTapOut()
{
	mTap = make_shared<TapNode>();
	mLineIn->connect( mTap )->connect( mContext->getRoot() );
}

void InputTestApp::setupInTapProcessOut()
{
	mTap = make_shared<TapNode>();
	auto ringMod = make_shared<RingMod>();
	mLineIn->connect( mTap )->connect( ringMod )->connect( mContext->getRoot() );
}

void InputTestApp::initGraph()
{
	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (before)" << endl;
	printGraph( mContext );

	mContext->initialize();

	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (after)" << endl;
	printGraph( mContext );
}

void InputTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );

	mTestSelector.segments.push_back( "pass through" );
	mTestSelector.segments.push_back( "in tap out" );
	mTestSelector.segments.push_back( "in process out" );
	mTestSelector.segments.push_back( "in tap process out" );
	mTestSelector.currentSectionIndex = 1;

#if defined( CINDER_COCOA_TOUCH )
	mPlayButton.bounds = Rectf( 0, 0, 120, 60 );
	mTestSelector.bounds = Rectf( getWindowWidth() - 190, 0.0f, getWindowWidth(), 160.0f );
#else
	mPlayButton.bounds = Rectf( 0, 0, 200, 60 );
	mTestSelector.bounds = Rectf( getWindowCenter().x + 100, 0.0f, getWindowWidth(), 160.0f );
#endif

	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );

	gl::enableAlphaBlending();
}

void InputTestApp::processTap( Vec2i pos )
{
	if( mPlayButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );

	size_t currentIndex = mTestSelector.currentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.currentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V << "selected: " << currentTest << endl;

		bool running = mContext->isEnabled();
		mContext->uninitialize();

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
		initGraph();

		if( running )
			mContext->start();
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
