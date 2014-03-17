#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "cinder/audio2/Gen.h"
#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/Filter.h"
#include "cinder/audio2/CinderAssert.h"
#include "cinder/audio2/Debug.h"

#include "../../common/AudioTestGui.h"

// TODO NEXT: add gen enable button to test tails

using namespace ci;
using namespace ci::app;
using namespace std;

class NodeEffectTestApp : public AppNative {
  public:
	void setup();
	void draw();

	void setupOne();
	void setupForceStereo();
	void setupDownMix();
	void setupDelay();
	void setupFeedback();
	void setupEcho();
	void setupCycle();

	void makeNodes();
	void switchTest( const string &currentTest );
	void applyChirp();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	audio2::GenRef				mGen;
	audio2::GainRef				mGain;
	audio2::Pan2dRef			mPan;
	audio2::FilterLowPassRef	mLowPass;
	audio2::DelayRef			mDelay;

	vector<TestWidget *>	mWidgets;
	Button					mPlayButton, mGenButton, mChirpButton;
	VSelector				mTestSelector;
	HSlider					mGainSlider, mPanSlider, mLowPassFreqSlider, mFilterParam2Slider;
};

void NodeEffectTestApp::setup()
{
	auto ctx = audio2::master();

//	auto outputDevice = ctx->getOutput()->getDevice();

	auto lineOut = ctx->createLineOut();
	lineOut->getDevice()->updateFormat( audio2::Device::Format().framesPerBlock( 64 ) );
	ctx->setOutput( lineOut );

//	mGenButton.setEnabled( true ); // set to start with GenSine

	makeNodes();
	setupOne();
//	setupFeedback();

	setupUI();

	ctx->printGraph();
	CI_LOG_V( "Context samplerate: " << ctx->getSampleRate() << ", frames per block: " << ctx->getFramesPerBlock() );
}

void NodeEffectTestApp::makeNodes()
{
	auto ctx = audio2::master();

	mGain = ctx->makeNode( new audio2::Gain );
	mGain->setValue( 0.25f );

	mPan = ctx->makeNode( new audio2::Pan2d );

	CI_LOG_V( "gen button enabled: " << mGenButton.mEnabled );
	auto genFmt = audio2::Node::Format().autoEnable();
	if( mGenButton.mEnabled )
		mGen = ctx->makeNode( new audio2::GenSine( 220, genFmt ) );
	else
		mGen = ctx->makeNode( new audio2::GenNoise( genFmt ) );

	mLowPass = ctx->makeNode( new audio2::FilterLowPass() );
//	mLowPass = ctx->makeNode( new audio2::FilterHighPass() );

	mDelay = ctx->makeNode( new audio2::Delay );
	mDelay->setDelaySeconds( 0.5f );
//	mDelay->setDelaySeconds( 100.0f / (float)ctx->getSampleRate() );
}

void NodeEffectTestApp::setupOne()
{
	mGen >> mLowPass >> mGain >> mPan >> audio2::master()->getOutput();
}

// TODO: move to NodeTest
void NodeEffectTestApp::setupForceStereo()
{
	mGen >> mLowPass >> mGain >> mPan >> audio2::master()->getOutput();
}

// TODO: move to NodeTest
void NodeEffectTestApp::setupDownMix()
{
	auto ctx = audio2::master();
	auto mono = ctx->makeNode( new audio2::Gain( audio2::Node::Format().channels( 1 ) ) );
	mGen >> mLowPass >> mGain >> mPan >> mono >> ctx->getOutput();
}

void NodeEffectTestApp::setupDelay()
{
	mGen >> mGain >> mDelay >> audio2::master()->getOutput();
}

// sub-classed merely so printGraph prints a more descriptive name
struct FeedbackGain : public audio2::Gain {
	FeedbackGain( float val ) : Gain( val ) {}


};

void NodeEffectTestApp::setupFeedback()
{
	// delay + feedback
	auto ctx = audio2::master();

	auto feedbackGain = ctx->makeNode( new FeedbackGain( 0.5f ) );

#if 1
	mGen >> mGain >> mDelay >> feedbackGain >> mDelay >> ctx->getOutput();
//	mGen >> mDelay >> feedbackGain >> mDelay >> ctx->getOutput();
#else
	auto add = ctx->makeNode( new audio2::Add( 0.1f ) );
	add >> mDelay >> feedbackGain >> mDelay >> ctx->getOutput();
#endif
}

void NodeEffectTestApp::setupEcho()
{
	// a more complex feedback graph, but more accurate to what you'd use in the real world
	// - sends dry signal to output so you hear it immediately.

	auto feedbackGain = audio2::master()->makeNode( new FeedbackGain( 0.5f ) );

	mGen >> mGain;

	mGain >> audio2::master()->getOutput();										// dry
	mGain >> mDelay >> feedbackGain >> mDelay >> audio2::master()->getOutput(); // wet

//	mGen >> audio2::master()->getOutput();										// dry
//	mGen >> mDelay >> feedbackGain >> mDelay >> audio2::master()->getOutput(); // wet

}

void NodeEffectTestApp::setupCycle()
{
	// this throws NodeCycleExc

	try {
		mGen >> mLowPass >> mGain >> mLowPass;
		mLowPass->addConnection( audio2::master()->getOutput() );

		CI_ASSERT_NOT_REACHABLE();
	}
	catch( audio2::NodeCycleExc &exc ) {
		CI_LOG_E( "audio2::NodeCycleExc, what: " << exc.what() );
	}
}

void NodeEffectTestApp::applyChirp()
{
	mGen->getParamFreq()->applyRamp( 440, 00, 0.15f );
}

void NodeEffectTestApp::setupUI()
{
	const float padding = 10;
	Rectf buttonRect( 0, 0, 200, 60 );

	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.mBounds = buttonRect;
	mWidgets.push_back( &mPlayButton );

	buttonRect += Vec2f( 0, buttonRect.getHeight() + padding );
	mGenButton.mIsToggle = true;
	mGenButton.mTitleNormal = "noise";
	mGenButton.mTitleEnabled = "sine";
	mGenButton.mBounds = buttonRect;
	mWidgets.push_back( &mGenButton );

	buttonRect += Vec2f( 0, buttonRect.getHeight() + padding );
	mChirpButton = Button( false, "chirp" );
	mChirpButton.mBounds = buttonRect;
	mWidgets.push_back( &mChirpButton );

	mTestSelector.mSegments.push_back( "one" );
	mTestSelector.mSegments.push_back( "force stereo" );
	mTestSelector.mSegments.push_back( "down-mix" );
	mTestSelector.mSegments.push_back( "delay" );
	mTestSelector.mSegments.push_back( "feedback" );
	mTestSelector.mSegments.push_back( "echo" );
	mTestSelector.mSegments.push_back( "cycle" );
	mTestSelector.mBounds = Rectf( (float)getWindowWidth() * 0.67f, 0, (float)getWindowWidth(), 160 );
	mWidgets.push_back( &mTestSelector );

	float width = std::min( (float)getWindowWidth() - 20,  440.0f );
	Rectf sliderRect( getWindowCenter().x - width / 2, 200, getWindowCenter().x + width / 2, 250 );
	mGainSlider.mBounds = sliderRect;
	mGainSlider.mTitle = "Gain";
	mGainSlider.set( mGain->getValue() );
	mWidgets.push_back( &mGainSlider );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + padding );
	mPanSlider.mBounds = sliderRect;
	mPanSlider.mTitle = "Pan";
	mPanSlider.set( mPan->getPos() );
	mWidgets.push_back( &mPanSlider );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + padding );
	mLowPassFreqSlider.mBounds = sliderRect;
	mLowPassFreqSlider.mTitle = "LowPass Freq";
	mLowPassFreqSlider.mMax = 1000;
	mLowPassFreqSlider.set( mLowPass->getCutoffFreq() );
	mWidgets.push_back( &mLowPassFreqSlider );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + padding );
	mFilterParam2Slider.mBounds = sliderRect;
	mFilterParam2Slider.mTitle = "filter resonance";
	mFilterParam2Slider.mMax = 50;
	mFilterParam2Slider.set( mLowPass->getResonance() );
	mWidgets.push_back( &mFilterParam2Slider );

	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalMouseDrag().connect( [this] ( MouseEvent &event ) { processDrag( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );
	getWindow()->getSignalTouchesMoved().connect( [this] ( TouchEvent &event ) {
		for( const TouchEvent::Touch &touch : getActiveTouches() )
			processDrag( touch.getPos() );
	} );

	gl::enableAlphaBlending();
}

void NodeEffectTestApp::processDrag( Vec2i pos )
{
	if( mGainSlider.hitTest( pos ) )
		mGain->getParam()->applyRamp( mGainSlider.mValueScaled, 0.015f );
	if( mPanSlider.hitTest( pos ) )
		mPan->setPos( mPanSlider.mValueScaled );
	if( mLowPassFreqSlider.hitTest( pos ) )
		mLowPass->setCutoffFreq( mLowPassFreqSlider.mValueScaled );
	if( mFilterParam2Slider.hitTest( pos ) )
		mLowPass->setResonance( mFilterParam2Slider.mValueScaled );
}

void NodeEffectTestApp::processTap( Vec2i pos )
{
	auto ctx = audio2::master();

	if( mPlayButton.hitTest( pos ) )
		ctx->setEnabled( ! ctx->isEnabled() );
	else if( mGenButton.hitTest( pos ) ) {
		makeNodes();
		switchTest( mTestSelector.currentSection() );

		if( mGenButton.mEnabled )
			applyChirp();
	}
	else if( mChirpButton.hitTest( pos ) )
		applyChirp();


	size_t currentIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		CI_LOG_V( "selected: " << currentTest );
		switchTest( currentTest );
	}

	processDrag( pos );
}

void NodeEffectTestApp::switchTest( const string &currentTest )
{
	auto ctx = audio2::master();

	bool enabled = ctx->isEnabled();
	ctx->stop();

	ctx->disconnectAllNodes();

	if( currentTest == "one" )
		setupOne();
	else if( currentTest == "force stereo" )
		setupForceStereo();
	else if( currentTest == "down-mix" )
		setupDownMix();
	else if( currentTest == "delay" )
		setupDelay();
	else if( currentTest == "feedback" )
		setupFeedback();
	else if( currentTest == "echo" )
		setupEcho();
	else if( currentTest == "cycle" )
		setupCycle();

	ctx->setEnabled( enabled );
	ctx->printGraph();
}

void NodeEffectTestApp::draw()
{
	gl::clear();
	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( NodeEffectTestApp, RendererGl )
