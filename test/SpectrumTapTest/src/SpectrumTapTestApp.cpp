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

//#define SOUND_FILE "tone440.wav"
//#define SOUND_FILE "tone440L220R.wav"
//#define SOUND_FILE "Blank__Kytt_-_08_-_RSPN.mp3"
#define SOUND_FILE "cash_satisfied_mind.mp3"

#define FFT_SIZE 2048
#define WINDOW_SIZE 1024
#define WINDOW_TYPE WindowType::BLACKMAN

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace audio2;

class SpectrumTapTestApp : public AppNative {
  public:
	void prepareSettings( Settings *settings );
	void fileDrop( FileDropEvent event );
	void setup();
	void update();
	void draw();

	void initContext();
	void setupUI();
	void processTap( Vec2i pos );
	void processDrag( Vec2i pos );
	void printBinFreq( size_t xPos );

	ContextRef mContext;
	PlayerNodeRef mPlayerNode;

	SpectrumTapNodeRef mSpectrumTap;

	vector<TestWidget *> mWidgets;
	Button mEnableGraphButton, mPlaybackButton, mLoopButton, mApplyWindowButton, mScaleDecibelsButton;
	HSlider mSmoothingFactorSlider;
	bool mScaleDecibels;
	float mSpectroMargin;
};


void SpectrumTapTestApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1200, 500 );
}

void SpectrumTapTestApp::setup()
{
	mScaleDecibels = true;
	mSpectroMargin = 40.0f;

	mContext = Context::instance()->createContext();

	DataSourceRef dataSource = loadResource( SOUND_FILE );
	auto sourceFile = SourceFile::create( dataSource, 0, 44100 );
	LOG_V << "output samplerate: " << sourceFile->getSampleRate() << endl;

	auto audioBuffer = sourceFile->loadBuffer();

	LOG_V << "loaded source buffer, frames: " << audioBuffer->getNumFrames() << endl;


	mPlayerNode = make_shared<BufferPlayerNode>( audioBuffer );
	mSpectrumTap = make_shared<SpectrumTapNode>( FFT_SIZE, WINDOW_SIZE, WINDOW_TYPE );

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
	Rectf buttonRect( 0.0f, 0.0f, 200.0f, mSpectroMargin - 2.0f );
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

	Vec2f sliderSize( 200.0f, 30.0f );
	Rectf sliderRect( getWindowWidth() - sliderSize.x - mSpectroMargin, buttonRect.y2 + padding, getWindowWidth() - mSpectroMargin, buttonRect.y2 + padding + sliderSize.y );
	mSmoothingFactorSlider.bounds = sliderRect;
	mSmoothingFactorSlider.title = "Smoothing";
	mSmoothingFactorSlider.min = 0.0f;
	mSmoothingFactorSlider.max = 1.0f;
	mSmoothingFactorSlider.set( mSpectrumTap->getSmoothingFactor() );
	mWidgets.push_back( &mSmoothingFactorSlider );


	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );
	getWindow()->getSignalMouseDrag().connect( [this] ( MouseEvent &event ) { processDrag( event.getPos() ); } );
	getWindow()->getSignalTouchesMoved().connect( [this] ( TouchEvent &event ) {
		for( const TouchEvent::Touch &touch : getActiveTouches() )
			processDrag( touch.getPos() );
	} );

	gl::enableAlphaBlending();
}

void SpectrumTapTestApp::printBinFreq( size_t xPos )
{
	if( xPos < mSpectroMargin || xPos > getWindowWidth() - mSpectroMargin )
		return;

//	freq = bin * samplerate / sizeFft

	size_t numBins = mSpectrumTap->getFftSize() / 2;
	size_t spectroWidth = getWindowWidth() - mSpectroMargin * 2;
	size_t bin = ( numBins * ( xPos - mSpectroMargin ) ) / spectroWidth;
	float freq = bin * mContext->getSampleRate() / float( mSpectrumTap->getFftSize() );

	LOG_V << "bin: " << bin << ", freq: " << freq << endl;
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
		printBinFreq( pos.x );
}

void SpectrumTapTestApp::processDrag( Vec2i pos )
{
	if( mSmoothingFactorSlider.hitTest( pos ) )
		mSpectrumTap->setSmoothingFactor( mSmoothingFactorSlider.valueScaled );
}

void SpectrumTapTestApp::fileDrop( FileDropEvent event )
{
	const fs::path &filePath = event.getFile( 0 );
	LOG_V << "File dropped: " << filePath << endl;

	DataSourceRef dataSource = loadFile( filePath );
	auto sourceFile = SourceFile::create( dataSource, 0, 44100 );
	LOG_V << "output samplerate: " << sourceFile->getSampleRate() << endl;

	auto audioBuffer = sourceFile->loadBuffer();

	LOG_V << "loaded source buffer, frames: " << audioBuffer->getNumFrames() << endl;


	bool running = mContext->isEnabled();
	mContext->uninitialize();

	mPlayerNode->disconnect();
	mPlayerNode = make_shared<BufferPlayerNode>( audioBuffer );
	mPlayerNode->connect( mSpectrumTap );

	initContext();
	if( running )
		mContext->start();
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
	float padding = 0.0f;
	float binWidth = ( (float)getWindowWidth() - mSpectroMargin * 2.0f - padding * ( numBins - 1 ) ) / (float)numBins;
	float binYScaler = ( (float)getWindowHeight() - mSpectroMargin * 2.0f );

	Rectf bin( mSpectroMargin, getWindowHeight() - mSpectroMargin, mSpectroMargin + binWidth, getWindowHeight() - mSpectroMargin );
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

	// draw rect around spectrogram boundary
	gl::color( Color::gray( 0.5 ) );
	gl::drawStrokedRect( Rectf( mSpectroMargin, mSpectroMargin, getWindowWidth() - mSpectroMargin, getWindowHeight() - mSpectroMargin ) );

	auto min = min_element( mag.begin(), mag.end() );
	auto max = max_element( mag.begin(), mag.end() );

	string info = string( "min: " ) + toString( *min ) + string( ", max: " ) + toString( *max );
	gl::drawString( info, Vec2f( mSpectroMargin, getWindowHeight() - 30.0f ) );

	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( SpectrumTapTestApp, RendererGl )
