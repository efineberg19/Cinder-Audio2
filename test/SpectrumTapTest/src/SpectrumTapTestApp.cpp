#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Utilities.h"

#include "audio2/audio.h"
#include "audio2/TapNode.h"
#include "audio2/GeneratorNode.h"
#include "audio2/Debug.h"
#include "audio2/Dsp.h"

#include "Gui.h"

#include <Accelerate/Accelerate.h>

#define SOUND_FILE "tone440.wav"
//#define SOUND_FILE "tone440L220R.wav"
//#define SOUND_FILE "Blank__Kytt_-_08_-_RSPN.mp3"


using namespace ci;
using namespace ci::app;
using namespace std;

using namespace audio2;

class SpectrumTapTestApp : public AppNative {
  public:
	void prepareSettings( Settings *settings );
	void setup();
	void update();
	void draw();

	void initContext();
	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );
	void seek( size_t xPos );

	ContextRef mContext;
	PlayerNodeRef mPlayerNode;
	SourceFileRef mSourceFile;

	SpectrumTapNodeRef mSpectrumTap;

	vector<TestWidget *> mWidgets;
	Button mEnableGraphButton, mPlaybackButton, mLoopButton, mApplyWindowButton, mScaleDecibelsButton;
	bool mScaleDecibels;
};


void SpectrumTapTestApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1200, 500 );
}

void SpectrumTapTestApp::setup()
{
	mScaleDecibels = true;
	
	// TODO: convert to unit tests
	LOG_V << "toDecibels( 0 ) = " << toDecibels( 0.0f ) << endl;
	LOG_V << "toDecibels( 0.5 ) = " << toDecibels( 0.5f ) << endl;
	LOG_V << "toDecibels( 1.0 ) = " << toDecibels( 1.0f ) << endl;

	LOG_V << "toLinear( 0 ) = " << toLinear( 0.0f ) << endl;
	LOG_V << "toLinear( 90.0f ) = " << toLinear( 90.0f ) << endl;
	LOG_V << "toLinear( 100.0f ) = " << toLinear( 100.0f ) << endl;

	mContext = Context::instance()->createContext();

	DataSourceRef dataSource = loadResource( SOUND_FILE );
	mSourceFile = SourceFile::create( dataSource, 0, 44100 );
	LOG_V << "output samplerate: " << mSourceFile->getSampleRate() << endl;

	auto audioBuffer = mSourceFile->loadBuffer();

	LOG_V << "loaded source buffer, frames: " << audioBuffer->getNumFrames() << endl;


	mPlayerNode = make_shared<BufferPlayerNode>( audioBuffer );

	mSpectrumTap = make_shared<SpectrumTapNode>( 1024 );

	mPlayerNode->connect( mSpectrumTap )->connect( mContext->getRoot() );

	initContext();
	setupUI();

	mSpectrumTap->start();
	mContext->start();
	mEnableGraphButton.setEnabled( true );

	mApplyWindowButton.setEnabled( mSpectrumTap->isWindowingEnabled() );
	mScaleDecibelsButton.setEnabled( mScaleDecibels );
}

void SpectrumTapTestApp::initContext()
{
	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (before)" << endl;
	printGraph( mContext );

	mContext->initialize();

	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (after)" << endl;
	printGraph( mContext );
}

void SpectrumTapTestApp::setupUI()
{
	Rectf buttonRect( 0.0f, 0.0f, 200.0f, 60.0f );
	float padding = 10.0f;
	mEnableGraphButton.isToggle = true;
	mEnableGraphButton.titleNormal = "graph off";
	mEnableGraphButton.titleEnabled = "graph on";
	mEnableGraphButton.bounds = buttonRect;
	mWidgets.push_back( &mEnableGraphButton );

	buttonRect += Vec2f( buttonRect.getWidth() + padding, 0.0f );
	mPlaybackButton.isToggle = true;
	mPlaybackButton.titleNormal = "play sample";
	mPlaybackButton.titleEnabled = "stop sample";
	mPlaybackButton.bounds = buttonRect;
	mWidgets.push_back( &mPlaybackButton );

	buttonRect += Vec2f( buttonRect.getWidth() + padding, 0.0f );
	mLoopButton.isToggle = true;
	mLoopButton.titleNormal = "loop off";
	mLoopButton.titleEnabled = "loop on";
	mLoopButton.bounds = buttonRect;
	mWidgets.push_back( &mLoopButton );

	buttonRect += Vec2f( buttonRect.getWidth() + padding, 0.0f );
	mApplyWindowButton.isToggle = true;
	mApplyWindowButton.titleNormal = "apply window";
	mApplyWindowButton.titleEnabled = "apply window";
	mApplyWindowButton.bounds = buttonRect;
	mWidgets.push_back( &mApplyWindowButton );

	buttonRect += Vec2f( buttonRect.getWidth() + padding, 0.0f );
	mScaleDecibelsButton.isToggle = true;
	mScaleDecibelsButton.titleNormal = "linear";
	mScaleDecibelsButton.titleEnabled = "decibels";
	mScaleDecibelsButton.bounds = buttonRect;
	mWidgets.push_back( &mScaleDecibelsButton );

	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalMouseDrag().connect( [this] ( MouseEvent &event ) { processDrag( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );
	getWindow()->getSignalTouchesMoved().connect( [this] ( TouchEvent &event ) {
		for( const TouchEvent::Touch &touch : getActiveTouches() )
			processDrag( touch.getPos() );
	} );

	gl::enableAlphaBlending();
}

void SpectrumTapTestApp::seek( size_t xPos )
{
	size_t seek = mPlayerNode->getNumFrames() * xPos / getWindowWidth();
	mPlayerNode->setReadPosition( seek );
}

void SpectrumTapTestApp::processDrag( Vec2i pos )
{
	seek( pos.x );
}

// TODO: currently makes sense to enable processor + tap together - consider making these enabled together.
// - possible solution: add a silent flag that is settable by client
void SpectrumTapTestApp::processTap( Vec2i pos )
{
	if( mEnableGraphButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );
	else if( mPlaybackButton.hitTest( pos ) )
		mPlayerNode->setEnabled( ! mPlayerNode->isEnabled() );
	else if( mLoopButton.hitTest( pos ) )
		mPlayerNode->setLoop( ! mPlayerNode->getLoop() );
	else if( mApplyWindowButton.hitTest( pos ) )
		mSpectrumTap->setWindowingEnabled( ! mSpectrumTap->isWindowingEnabled() );
	else if( mScaleDecibelsButton.hitTest( pos ) )
		mScaleDecibels = ! mScaleDecibels;
	else
		seek( pos.x );
}

void SpectrumTapTestApp::update()
{
	// update playback button, since the player node may stop itself at the end of a file.
	if( ! mPlayerNode->isEnabled() )
		mPlaybackButton.setEnabled( false );
}

void SpectrumTapTestApp::draw()
{
	gl::clear();

	// draw magnitude spectrum bins

	auto &mag = mSpectrumTap->getMagSpectrum();
	size_t numBins = mag.size();
	float margin = 40.0f;
	float padding = 0.0f;
	float binWidth = ( (float)getWindowWidth() - margin * 2.0f - padding * ( numBins - 1 ) ) / (float)numBins;
	float binYScaler = ( (float)getWindowHeight() - margin * 2.0f );

	Rectf bin( margin, getWindowHeight() - margin, margin + binWidth, getWindowHeight() - margin );
	for( size_t i = 0; i < numBins; i++ ) {
		float h = mag[i];
		if( mScaleDecibels ) {
			h = toDecibels( h ) / 100.0f;
//			if( h < 0.3f )
//				h = 0.0f;
		}
		bin.y1 = bin.y2 - h * binYScaler;
		gl::color( 0.0f, 0.9f, 0.0f );
		gl::drawSolidRect( bin );

		bin += Vec2f( binWidth + padding, 0.0f );
	}

	auto min = min_element( mag.begin(), mag.end() );
	auto max = max_element( mag.begin(), mag.end() );

	string info = string( "min: " ) + toString( *min ) + string( ", max: " ) + toString( *max );
	gl::drawString( info, Vec2f( margin, getWindowHeight() - 30.0f ) );

	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( SpectrumTapTestApp, RendererGl )
