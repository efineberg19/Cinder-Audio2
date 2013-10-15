#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Timeline.h"

#include "audio2/Context.h"
#include "audio2/audio.h"
#include "audio2/NodeEffect.h"
#include "audio2/Scope.h"
#include "audio2/Dsp.h"
#include "audio2/Debug.h"

#include "Gui.h"
#include "Plot.h"

// TODO: test multiple formats for input
// - make sure inputs and outputs with different samplerates somehow works correctly (which was default for my win8 laptop)
// - requires input on windows to use a Converter

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

	void printDevices();

	void setupPassThrough();
	void setupInProcessOut();
	void setupInTapOut();
	void setupInTapProcessOut();

	Context* mContext;
	NodeLineInRef mLineIn;
	NodeLineOutRef mLineOut;
	ScopeRef mScope;

	VSelector mTestSelector;
	Button mPlayButton;

	Anim<float> mUnderrunFade, mOverrunFade;
	Rectf mUnderrunRect, mOverrunRect;
};

void InputTestApp::setup()
{
	mContext = Context::master();

	mLineIn = mContext->createLineIn();
	mLineOut = mContext->createLineOut();

	mLineIn->setAutoEnabled();

	setupInTapOut();

	setupUI();

	mContext->printGraph();
}

void InputTestApp::printDevices()
{
	LOG_V << "input device name: " << mLineIn->getDevice()->getName() << endl;
	console() << "\t channels: " << mLineIn->getDevice()->getNumInputChannels() << endl;
	console() << "\t samplerate: " << mLineIn->getDevice()->getSampleRate() << endl;
	console() << "\t block size: " << mLineIn->getDevice()->getFramesPerBlock() << endl;

	LOG_V << "output device name: " << mLineOut->getDevice()->getName() << endl;
	console() << "\t channels: " << mLineOut->getDevice()->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << mLineOut->getDevice()->getSampleRate() << endl;
	console() << "\t block size: " << mLineOut->getDevice()->getFramesPerBlock() << endl;

	LOG_V << "input == output: " << boolalpha << ( mLineIn->getDevice() == mLineOut->getDevice() ) << dec << endl;
}

void InputTestApp::setupPassThrough()
{
	mLineIn->connect( mContext->getTarget() );
}

void InputTestApp::setupInProcessOut()
{
	auto ringMod = mContext->makeNode( new RingMod() );
	mLineIn->connect( ringMod )->connect( mContext->getTarget() );
}

void InputTestApp::setupInTapOut()
{
	mScope = mContext->makeNode( new Scope( Scope::Format().windowSize( 1024 ) ) );
	mLineIn->connect( mScope )->connect( mContext->getTarget() );
}

void InputTestApp::setupInTapProcessOut()
{
	mScope = mContext->makeNode( new Scope() );
	auto ringMod = mContext->makeNode( new RingMod() );
	mLineIn->connect( mScope )->connect( ringMod )->connect( mContext->getTarget() );
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

	mUnderrunFade = mOverrunFade = 0.0f;

	Vec2i xrunSize( 80, 26 );
	mUnderrunRect = Rectf( getWindowWidth() - xrunSize.x, getWindowHeight() - xrunSize.y, getWindowWidth(), getWindowHeight() );
	mOverrunRect = mUnderrunRect - Vec2f( xrunSize.x + 10, 0 );

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
	const float xrunFadeTime = 1.3f;

	if( mLineIn->getLastUnderrun() )
		timeline().apply( &mUnderrunFade, 1.0f, 0.0f, xrunFadeTime );
	if( mLineIn->getLastOverrun() )
		timeline().apply( &mOverrunFade, 1.0f, 0.0f, xrunFadeTime );
}

void InputTestApp::draw()
{
	gl::clear();
	gl::color( 0.0f, 0.9f, 0.0f );

	if( mScope && mScope->isInitialized() ) {

		drawAudioBuffer( mScope->getBuffer(), getWindowBounds() );

		float volumeMeterHeight = 20.0f;
		float volume = mScope->getVolume();
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

CINDER_APP_NATIVE( InputTestApp, RendererGl )
