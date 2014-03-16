#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "cinder/audio2/Gen.h"
#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/Filter.h"
#include "cinder/audio2/CinderAssert.h"
#include "cinder/audio2/Debug.h"

#include "../../common/AudioTestGui.h"

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
	Button					mPlayButton, mChirpButton;
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

	mGain = ctx->makeNode( new audio2::Gain() );
	mGain->setValue( 0.25f );

	mPan = ctx->makeNode( new audio2::Pan2d() );

	auto genFmt = audio2::Node::Format().autoEnable();
	mGen = ctx->makeNode( new audio2::GenNoise( genFmt ) );
//	mGen = ctx->makeNode( new audio2::GenSine( 220, genFmt );

	mLowPass = ctx->makeNode( new audio2::FilterLowPass() );
//	mLowPass = ctx->makeNode( new audio2::FilterHighPass() );

	mDelay = ctx->makeNode( new audio2::Delay );

	mDelay->setDelaySeconds( 1.0f );

	setupOne();

	setupUI();

	ctx->printGraph();
	CI_LOG_V( "Context samplerate: " << ctx->getSampleRate() << ", frames per block: " << ctx->getFramesPerBlock() );
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

void NodeEffectTestApp::setupFeedback()
{
	// delay + feedback

	auto feedbackGain = audio2::master()->makeNode( new audio2::Gain( 0.5f ) );

	mGen >> mGain >> mDelay >> feedbackGain >> mDelay >> audio2::master()->getOutput();
}

void NodeEffectTestApp::setupEcho()
{
	// a more complex feedback graph, but more accurate to what you'd use in the real world
	// (you immediately hear the original sound

	auto feedbackGain = audio2::master()->makeNode( new audio2::Gain( 0.5f ) );

	mGen >> mGain;

	mGain >> audio2::master()->getOutput();										// dry
	mGain >> mDelay >> feedbackGain >> mDelay >> audio2::master()->getOutput(); // wet
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
		CI_LOG_E( "audio2::NodeCycleExc: " << exc.what() );
	}
}

void NodeEffectTestApp::applyChirp()
{
	mGen->getParamFreq()->applyRamp( 220, 0, 0.15f );
}

void NodeEffectTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.mBounds = Rectf( 0, 0, 200, 60 );
	mWidgets.push_back( &mPlayButton );

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

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mPanSlider.mBounds = sliderRect;
	mPanSlider.mTitle = "Pan";
	mPanSlider.set( mPan->getPos() );
	mWidgets.push_back( &mPanSlider );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mLowPassFreqSlider.mBounds = sliderRect;
	mLowPassFreqSlider.mTitle = "LowPass Freq";
	mLowPassFreqSlider.mMax = 1000;
	mLowPassFreqSlider.set( mLowPass->getCutoffFreq() );
	mWidgets.push_back( &mLowPassFreqSlider );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
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

	size_t currentIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		CI_LOG_V( "selected: " << currentTest );

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

	processDrag( pos );
}

void NodeEffectTestApp::draw()
{
	gl::clear();
	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( NodeEffectTestApp, RendererGl )
