#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Rand.h"
#include "cinder/Timeline.h"

#include "cinder/audio2/NodeSource.h"
#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/Filter.h"
#include "cinder/audio2/CinderAssert.h"
#include "cinder/audio2/Debug.h"

#include "../../common/AudioTestGui.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class ParamTestApp : public AppNative {
  public:
	void setup();
	void update();
	void draw();
	void keyDown( KeyEvent event );

	void setupBasic();
	void setupFilter();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	void testApply();
	void testApply2();
	void testAppend();
	void testDelay();
	void testAppendCancel();
	void testModulator();

	void writeParamEval( audio2::Param *param );

	audio2::GenRef				mGen;
	audio2::GainRef				mGain;
	audio2::Pan2dRef			mPan;
	audio2::FilterLowPassRef	mLowPass;

	vector<TestWidget *>	mWidgets;
	Button					mPlayButton, mApplyButton, mApplyAppendButton, mAppendButton, mDelayButton, mModulatorButton, mAppendCancelButton;
	VSelector				mTestSelector;
	HSlider					mGainSlider, mPanSlider, mLowPassFreqSlider, mGenFreqSlider;
};

void ParamTestApp::setup()
{
	// example code posed to afb
//	Anim<Vec2f> pos = Vec2f( 1, 1 );
//	timeline().apply( &pos, Vec2f::zero(), 1, EaseInQuad() );
//	timeline().appendRamp( &pos, Vec2f( 10, 20 ), 2 ); // ???: Tween's mStartValue = (1,1) here?

	auto ctx = audio2::Context::master();
	mGain = ctx->makeNode( new audio2::Gain() );
	mGain->setValue( 0.8 );

	mPan = ctx->makeNode( new audio2::Pan2d() );

//	mGen = ctx->makeNode( new audio2::GenSine() );
	mGen = ctx->makeNode( new audio2::GenTriangle() );
//	mGen = ctx->makeNode( new audio2::GenPhasor() );

	mGen->setFreq( 220 );

	mLowPass = ctx->makeNode( new audio2::FilterLowPass() );

	setupBasic();

	setupUI();

	ctx->printGraph();

	testApply();
//	testApply2();
//	connectModulator();
}

void ParamTestApp::setupBasic()
{
	mGen->connect( mGain )->connect( audio2::Context::master()->getTarget() );
	mGen->start();
}

void ParamTestApp::setupFilter()
{
	mGen->connect( mLowPass )->connect( mGain )->connect( mPan )->connect( audio2::Context::master()->getTarget() );
	mGen->start();
}

void ParamTestApp::testApply()
{
	// (a): ramp volume to 0.7 of 0.2 seconds
//	mGain->getParam()->applyRamp( 0.7f, 0.2f );

	mGen->getParamFreq()->applyRamp( 220, 440, 1 );

	// PSEUDO CODE: possible syntax where context keeps references to Params, calling updateValueArray() (or just process() ?) on them each block:
	// - problem I have with this right now is that its alot more syntax for the common case (see: (a)) of ramping up volume
//	Context::master()->timeline()->apply( mGen->getParamFreq(), 220, 440, 1 );
	// - a bit shorter:
//	audio2::timeline()->apply( mGen->getParamFreq(), 220, 440, 1 );

	LOG_V( "num ramps: " << mGen->getParamFreq()->getNumRamps() );
}

// 2 events - first apply the ramp, blowing away anything else, then append another event to happen after that
void ParamTestApp::testApply2()
{
	mGen->getParamFreq()->applyRamp( 220, 880, 1 );
	mGen->getParamFreq()->appendRamp( 369.994f, 1 ); // F#4

	LOG_V( "num ramps: " << mGen->getParamFreq()->getNumRamps() );

//	writeParamEval( mGen->getParamFreq() );
}

// append an event with random frequency and duration 1 second, allowing them to build up. new events begin from the end of the last event
void ParamTestApp::testAppend()
{
	mGen->getParamFreq()->appendRamp( randFloat( 50, 800 ), 1.0f );

	LOG_V( "num ramps: " << mGen->getParamFreq()->getNumRamps() );
}

// make a ramp after a 1 second delay
void ParamTestApp::testDelay()
{
	mGen->getParamFreq()->applyRamp( 50, 440, 1, audio2::Param::Options().delay( 1 ) );
	LOG_V( "num ramps: " << mGen->getParamFreq()->getNumRamps() );
}

// apply a ramp from 220 to 880 over 2 seconds and then after a 1 second delay, cancel it. result should be ~ 550: 220 + (880 - 220) / 2.
void ParamTestApp::testAppendCancel()
{
	audio2::RampRef ramp = mGen->getParamFreq()->applyRamp( 220, 880, 2 );

	LOG_V( "num ramps: " << mGen->getParamFreq()->getNumRamps() );

	timeline().add( [ramp] {
		LOG_V( "canceling." );
		ramp->cancel();
	}, getElapsedSeconds() + 1 );
}

void ParamTestApp::testModulator()
{
	auto ctx = audio2::Context::master();
	auto mod = ctx->makeNode( new audio2::GenSine( audio2::Node::Format().autoEnable() ) );
	mod->setFreq( 2 );

	mGain->getParam()->setModulator( mod );
}

void ParamTestApp::setupUI()
{
	const float padding = 10.0f;

	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.mBounds = Rectf( 0, 0, 200, 60 );
	mWidgets.push_back( &mPlayButton );

	Rectf paramButtonRect( 0, mPlayButton.mBounds.y2 + padding, 120, mPlayButton.mBounds.y2 + padding + 40 );
	mApplyButton = Button( false, "apply" );
	mApplyButton.mBounds = paramButtonRect;
	mWidgets.push_back( &mApplyButton );

	paramButtonRect += Vec2f( paramButtonRect.getWidth() + padding, 0 );
	mApplyAppendButton = Button( false, "apply 2" );
	mApplyAppendButton.mBounds = paramButtonRect;
	mWidgets.push_back( &mApplyAppendButton );

	paramButtonRect += Vec2f( paramButtonRect.getWidth() + padding, 0 );
	mAppendButton = Button( false, "append" );
	mAppendButton.mBounds = paramButtonRect;
	mWidgets.push_back( &mAppendButton );

	paramButtonRect = mApplyButton.mBounds + Vec2f( 0, mApplyButton.mBounds.getHeight() + padding );
	mDelayButton = Button( false, "delay" );
	mDelayButton.mBounds = paramButtonRect;
	mWidgets.push_back( &mDelayButton );

	paramButtonRect += Vec2f( paramButtonRect.getWidth() + padding, 0 );
	mModulatorButton = Button( false, "modulator" );
	mModulatorButton.mBounds = paramButtonRect;
	mWidgets.push_back( &mModulatorButton );

	paramButtonRect += Vec2f( paramButtonRect.getWidth() + padding, 0 );
	mAppendCancelButton = Button( false, "cancel" );
	mAppendCancelButton.mBounds = paramButtonRect;
	mWidgets.push_back( &mAppendCancelButton );

	mTestSelector.mSegments.push_back( "basic" );
	mTestSelector.mSegments.push_back( "filter" );
	mTestSelector.mBounds = Rectf( getWindowWidth() * 0.67f, 0.0f, getWindowWidth(), 160.0f );
	mWidgets.push_back( &mTestSelector );

	float width = std::min( (float)getWindowWidth() - 20.0f,  440.0f );
	Rectf sliderRect( getWindowCenter().x - width / 2.0f, 200, getWindowCenter().x + width / 2.0f, 250 );
	mGainSlider.mBounds = sliderRect;
	mGainSlider.mTitle = "Gain";
	mGainSlider.set( mGain->getValue() );
	mWidgets.push_back( &mGainSlider );

	sliderRect += Vec2f( 0.0f, sliderRect.getHeight() + 10.0f );
	mPanSlider.mBounds = sliderRect;
	mPanSlider.mTitle = "Pan";
	mPanSlider.set( mPan->getPos() );
	mWidgets.push_back( &mPanSlider );

	sliderRect += Vec2f( 0.0f, sliderRect.getHeight() + 10.0f );
	mGenFreqSlider.mBounds = sliderRect;
	mGenFreqSlider.mTitle = "Gen Freq";
	mGenFreqSlider.mMin = 0.0f;
	mGenFreqSlider.mMax = 1200.0f;
	mGenFreqSlider.set( mGen->getFreq() );
	mWidgets.push_back( &mGenFreqSlider );

	sliderRect += Vec2f( 0.0f, sliderRect.getHeight() + 10.0f );
	mLowPassFreqSlider.mBounds = sliderRect;
	mLowPassFreqSlider.mTitle = "LowPass Freq";
	mLowPassFreqSlider.mMax = 1000.0f;
	mLowPassFreqSlider.set( mLowPass->getCutoffFreq() );
	mWidgets.push_back( &mLowPassFreqSlider );

	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalMouseDrag().connect( [this] ( MouseEvent &event ) { processDrag( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );
	getWindow()->getSignalTouchesMoved().connect( [this] ( TouchEvent &event ) {
		for( const TouchEvent::Touch &touch : getActiveTouches() )
			processDrag( touch.getPos() );
	} );

	gl::enableAlphaBlending();
}

void ParamTestApp::processDrag( Vec2i pos )
{
	if( mGainSlider.hitTest( pos ) ) {
//		mGain->setValue( mGainSlider.mValueScaled );
//		mGain->getParam()->applyRamp( mGainSlider.mValueScaled );
		LOG_V( "applying ramp on gain from: " << mGain->getValue() << " to: " << mGainSlider.mValueScaled );
		mGain->getParam()->applyRamp( mGainSlider.mValueScaled, 0.15f );
	}
	if( mPanSlider.hitTest( pos ) )
		mPan->setPos( mPanSlider.mValueScaled );
	if( mGenFreqSlider.hitTest( pos ) ) {
//		mGen->setFreq( mGenFreqSlider.mValueScaled );
//		mGen->getParamFreq()->applyRamp( mGenFreqSlider.mValueScaled, 0.3f );
		mGen->getParamFreq()->applyRamp( mGenFreqSlider.mValueScaled, 0.3f, audio2::Param::Options().rampFn( &audio2::rampOutQuad ) );
	}
	if( mLowPassFreqSlider.hitTest( pos ) )
		mLowPass->setCutoffFreq( mLowPassFreqSlider.mValueScaled );
}

void ParamTestApp::processTap( Vec2i pos )
{
	auto ctx = audio2::Context::master();
	size_t selectorIndex = mTestSelector.mCurrentSectionIndex;

	if( mPlayButton.hitTest( pos ) )
		ctx->setEnabled( ! ctx->isEnabled() );
	else if( mApplyButton.hitTest( pos ) )
		testApply();
	else if( mApplyAppendButton.hitTest( pos ) )
		testApply2();
	else if( mAppendButton.hitTest( pos ) )
		testAppend();
	else if( mDelayButton.hitTest( pos ) )
		testDelay();
	else if( mModulatorButton.hitTest( pos ) )
		testModulator();
	else if( mAppendCancelButton.hitTest( pos ) )
		testAppendCancel();
	else if( mTestSelector.hitTest( pos ) && selectorIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V( "selected: " << currentTest );

		bool enabled = ctx->isEnabled();
		ctx->stop();

		ctx->disconnectAllNodes();

		if( currentTest == "basic" )
			setupBasic();
		if( currentTest == "filter" )
			setupFilter();

		ctx->setEnabled( enabled );
		ctx->printGraph();
	}
	else
		processDrag( pos );
}

void ParamTestApp::keyDown( KeyEvent event )
{
	if( event.getCode() == KeyEvent::KEY_e )
		LOG_V( "mGen freq events: " << mGen->getParamFreq()->getNumRamps() );
}

void ParamTestApp::update()
{
	if( audio2::Context::master()->isEnabled() ) {
		mGainSlider.set( mGain->getValue() );
		mGenFreqSlider.set( mGen->getFreq() );
	}
}

void ParamTestApp::draw()
{
	gl::clear();
	drawWidgets( mWidgets );
}

// TODO: this will be formalized once there is an offline audio context and NodeTargetFile.
void ParamTestApp::writeParamEval( audio2::Param *param )
{
	auto ctx = audio2::Context::master();
	float duration = param->findDuration();
	float currTime = (float)ctx->getNumProcessedSeconds();
	size_t sampleRate = ctx->getSampleRate();
	audio2::Buffer audioBuffer( duration * sampleRate );

	param->eval( currTime, audioBuffer.getData(), audioBuffer.getSize(), sampleRate );

	auto target = audio2::TargetFile::create( "param.wav", sampleRate, 1 );
	target->write( &audioBuffer );

	LOG_V( "write complete" );
}

CINDER_APP_NATIVE( ParamTestApp, RendererGl )
