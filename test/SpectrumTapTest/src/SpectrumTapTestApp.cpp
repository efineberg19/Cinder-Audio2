#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Utilities.h"

#include "audio2/audio.h"
#include "audio2/TapNode.h"
#include "audio2/GeneratorNode.h"
#include "audio2/Debug.h"
#include "audio2/Dsp.h"

#include "Gui.h"
#include "Plot.h"
#include "Resources.h"

//#define SOUND_FILE "tone440.wav"
//#define SOUND_FILE "tone440L220R.wav"
//#define SOUND_FILE "Blank__Kytt_-_08_-_RSPN.mp3"
#define SOUND_FILE "cash_satisfied_mind.mp3"

// TODO: the goal is for all of these to be runtime configurable
#define FFT_SIZE 2048
#define WINDOW_SIZE 1024
#define WINDOW_TYPE WindowType::BLACKMAN

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace ci::audio2;

class SpectrumTapTestApp : public AppNative {
  public:
	void prepareSettings( Settings *settings );
	void fileDrop( FileDropEvent event );
	void setup();
	void update();
	void draw();

	void setupSine();
	void setupSample();
	void setupUI();
	void processTap( Vec2i pos );
	void processDrag( Vec2i pos );
	void printBinFreq( size_t xPos );


	ContextRef						mContext;
	BufferPlayerNodeRef				mPlayerNode;
	shared_ptr<UGenNode<SineGen> >	mSine;
	SpectrumTapNodeRef				mSpectrumTap;
	SourceFileRef					mSourceFile;

	vector<TestWidget *>			mWidgets;
	Button							mEnableGraphButton, mPlaybackButton, mLoopButton, mApplyWindowButton, mScaleDecibelsButton;
	VSelector						mTestSelector;
	HSlider							mSmoothingFactorSlider, mFreqSlider;
	SpectrumPlot					mSpectrumPlot;
	float							mSpectroMargin;
};


void SpectrumTapTestApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1200, 500 );
}

void SpectrumTapTestApp::setup()
{
	mSpectroMargin = 40.0f;

	mContext = Context::create();

	mSpectrumTap = mContext->makeNode( new SpectrumTapNode( SpectrumTapNode::Format().fftSize( FFT_SIZE ).windowSize( WINDOW_SIZE ).windowType( WINDOW_TYPE ) ) );
	mSpectrumTap->setAutoEnabled();

	mSine = mContext->makeNode( new UGenNode<SineGen>() );
	mSine->getUGen().setAmp( 0.25f );
	mSine->getUGen().setFreq( 440.0f );

#if ! defined( CINDER_MSW )
	// FIXME: audio decoding on msw not ready

	DataSourceRef dataSource = loadResource( RES_CASH_MP3 );
	mSourceFile = SourceFile::create( dataSource, 0, 44100 );
	LOG_V << "output samplerate: " << mSourceFile->getSampleRate() << endl;

	auto audioBuffer = mSourceFile->loadBuffer();
	LOG_V << "loaded source buffer, frames: " << audioBuffer->getNumFrames() << endl;

	mPlayerNode = mContext->makeNode( new BufferPlayerNode( audioBuffer ) );

#endif

	setupSine();

	setupUI();

	mContext->start();
	mEnableGraphButton.setEnabled( true );

	mApplyWindowButton.setEnabled( mSpectrumTap->isWindowingEnabled() );
	mScaleDecibelsButton.setEnabled( mSpectrumPlot.getScaleDecibels() );

	LOG_V << "------------------------- Graph configuration: -------------------------" << endl;
	printGraph( mContext );
}

void SpectrumTapTestApp::setupSine()
{
	mSine->connect( mSpectrumTap )->connect( mContext->getRoot() );
	if( mPlaybackButton.mEnabled )
		mSine->start();
}

void SpectrumTapTestApp::setupSample()
{
	mPlayerNode->connect( mSpectrumTap )->connect( mContext->getRoot() );
	if( mPlaybackButton.mEnabled )
		mPlayerNode->start();
}

void SpectrumTapTestApp::setupUI()
{
	Rectf buttonRect( 0.0f, 0.0f, 200.0f, mSpectroMargin - 2.0f );
	float padding = 10.0f;
	mEnableGraphButton.mIsToggle = true;
	mEnableGraphButton.mTitleNormal = "graph off";
	mEnableGraphButton.mTitleEnabled = "graph on";
	mEnableGraphButton.mBounds = buttonRect;
	mWidgets.push_back( &mEnableGraphButton );

	buttonRect += Vec2f( buttonRect.getWidth() + padding, 0.0f );
	mPlaybackButton.mIsToggle = true;
	mPlaybackButton.mTitleNormal = "play";
	mPlaybackButton.mTitleEnabled = "stop";
	mPlaybackButton.mBounds = buttonRect;
	mWidgets.push_back( &mPlaybackButton );

	buttonRect += Vec2f( buttonRect.getWidth() + padding, 0.0f );
	mLoopButton.mIsToggle = true;
	mLoopButton.mTitleNormal = "loop off";
	mLoopButton.mTitleEnabled = "loop on";
	mLoopButton.mBounds = buttonRect;
	mWidgets.push_back( &mLoopButton );

	buttonRect += Vec2f( buttonRect.getWidth() + padding, 0.0f );
	mApplyWindowButton.mIsToggle = true;
	mApplyWindowButton.mTitleNormal = "apply window";
	mApplyWindowButton.mTitleEnabled = "apply window";
	mApplyWindowButton.mBounds = buttonRect;
	mWidgets.push_back( &mApplyWindowButton );

	buttonRect += Vec2f( buttonRect.getWidth() + padding, 0.0f );
	mScaleDecibelsButton.mIsToggle = true;
	mScaleDecibelsButton.mTitleNormal = "linear";
	mScaleDecibelsButton.mTitleEnabled = "decibels";
	mScaleDecibelsButton.mBounds = buttonRect;
	mWidgets.push_back( &mScaleDecibelsButton );

	Vec2f sliderSize( 200.0f, 30.0f );
	Rectf selectorRect( getWindowWidth() - sliderSize.x - mSpectroMargin, buttonRect.y2 + padding, getWindowWidth() - mSpectroMargin, buttonRect.y2 + padding + sliderSize.y * 2 );
	mTestSelector.mSegments.push_back( "sine" );
	mTestSelector.mSegments.push_back( "sample" );
	mTestSelector.mBounds = selectorRect;
	mWidgets.push_back( &mTestSelector );

	Rectf sliderRect( selectorRect.x1, selectorRect.y2 + padding, selectorRect.x2, selectorRect.y2 + padding + sliderSize.y );
	mSmoothingFactorSlider.mBounds = sliderRect;
	mSmoothingFactorSlider.mTitle = "Smoothing";
	mSmoothingFactorSlider.mMin = 0.0f;
	mSmoothingFactorSlider.mMax = 1.0f;
	mSmoothingFactorSlider.set( mSpectrumTap->getSmoothingFactor() );
	mWidgets.push_back( &mSmoothingFactorSlider );

	sliderRect += Vec2f( 0.0f, sliderSize.y + padding );
	mFreqSlider.mBounds = sliderRect;
	mFreqSlider.mTitle = "Sine Freq";
	mFreqSlider.mMin = 0.0f;
	mFreqSlider.mMax = mContext->getSampleRate() / 2.0f;
	mFreqSlider.set( mSine->getUGen().getFreq() );
	mWidgets.push_back( &mFreqSlider );


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
	else if( mPlaybackButton.hitTest( pos ) ) {
		if( mTestSelector.currentSection() == "sine" )
			mSine->setEnabled( ! mSine->isEnabled() );
		else
			mPlayerNode->setEnabled( ! mPlayerNode->isEnabled() );
	}
	else if( mLoopButton.hitTest( pos ) )
		mPlayerNode->setLoop( ! mPlayerNode->getLoop() );
	else if( mApplyWindowButton.hitTest( pos ) )
		mSpectrumTap->setWindowingEnabled( ! mSpectrumTap->isWindowingEnabled() );
	else if( mScaleDecibelsButton.hitTest( pos ) )
		mSpectrumPlot.setScaleDecibels( ! mSpectrumPlot.getScaleDecibels() );
	else
		printBinFreq( pos.x );

	size_t currentIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V << "selected: " << currentTest << endl;

		bool enabled = mContext->isEnabled();
		mContext->disconnectAllNodes();

		if( currentTest == "sine" )
			setupSine();
		if( currentTest == "sample" )
			setupSample();

		mContext->setEnabled( enabled );
	}

}

void SpectrumTapTestApp::processDrag( Vec2i pos )
{
	if( mSmoothingFactorSlider.hitTest( pos ) )
		mSpectrumTap->setSmoothingFactor( mSmoothingFactorSlider.mValueScaled );
	if( mFreqSlider.hitTest( pos ) )
		mSine->getUGen().setFreq( mFreqSlider.mValueScaled );
}

void SpectrumTapTestApp::fileDrop( FileDropEvent event )
{
	const fs::path &filePath = event.getFile( 0 );
	LOG_V << "File dropped: " << filePath << endl;

	DataSourceRef dataSource = loadFile( filePath );
	mSourceFile = SourceFile::create( dataSource, 0, 44100 );
	LOG_V << "output samplerate: " << mSourceFile->getSampleRate() << endl;

	mPlayerNode->setBuffer( mSourceFile->loadBuffer() );

	LOG_V << "loaded and set new source buffer, frames: " << mSourceFile->getNumFrames() << endl;
}

void SpectrumTapTestApp::update()
{
	// update playback button, since the player node may stop itself at the end of a file.
	if( mTestSelector.currentSection() == "sample" && ! mPlayerNode->isEnabled() )
		mPlaybackButton.setEnabled( false );
}

void SpectrumTapTestApp::draw()
{
	gl::clear();

	// draw magnitude spectrum bins
	auto &mag = mSpectrumTap->getMagSpectrum();
	mSpectrumPlot.setBounds( Rectf( mSpectroMargin, mSpectroMargin, getWindowWidth() - mSpectroMargin, getWindowHeight() - mSpectroMargin ) );
	mSpectrumPlot.draw( mag );
	
	// draw rect around spectrogram boundary
	gl::color( Color::gray( 0.5 ) );
	gl::drawStrokedRect( mSpectrumPlot.getBounds() );

	if( ! mag.empty() ) {
		auto min = min_element( mag.begin(), mag.end() );
		auto max = max_element( mag.begin(), mag.end() );

		string info = string( "min: " ) + toString( *min ) + string( ", max: " ) + toString( *max );
		gl::drawString( info, Vec2f( mSpectroMargin, getWindowHeight() - 30.0f ) );
	}

	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( SpectrumTapTestApp, RendererGl )
