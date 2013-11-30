#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/NodeSource.h"
#include "audio2/NodeEffect.h"
#include "audio2/NodeFilter.h"
#include "audio2/CinderAssert.h"
#include "audio2/Debug.h"

#include "../../common/AudioTestGui.h"

//#include "cinder/Timeline.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace ci::audio2;

class ParamTestApp : public AppNative {
  public:
	void setup();
	void draw();
	void keyDown( KeyEvent event );

	void setupBasic();
	void setupFilter();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	void triggerApply();
	void triggerApply2();
	void triggerAppend();
	void triggerDelay();

	Context*				mContext;
	NodeGenRef				mGen;
	NodeGainRef				mGain;
	NodePan2dRef			mPan;
	NodeFilterLowPassRef	mLowPass;

	vector<TestWidget *>	mWidgets;
	Button					mPlayButton, mApplyButton, mApplyAppendButton, mAppendButton, mDelayButton;
	VSelector				mTestSelector;
	HSlider					mGainSlider, mPanSlider, mLowPassFreqSlider, mGenFreqSlider;
};

void ParamTestApp::setup()
{
	// example code posed to afb
//	Anim<Vec2f> pos = Vec2f( 1, 1 );
//	timeline().apply( &pos, Vec2f::zero(), 1, EaseInQuad() );
//	timeline().appendTo( &pos, Vec2f( 10, 20 ), 2 ); // ???: Tween's mStartValue = (1,1) here?

	mContext = Context::master();

	mGain = mContext->makeNode( new NodeGain() );
	mGain->setValue( 0.8 );

	mPan = mContext->makeNode( new NodePan2d() );

	mGen = mContext->makeNode( new NodeGenSine() );
//	mGen = mContext->makeNode( new NodeGenTriangle() );
//	mGen = mContext->makeNode( new NodeGenPhasor() );

	mGen->setFreq( 0 );

	mLowPass = mContext->makeNode( new NodeFilterLowPass() );

	setupBasic();

	setupUI();

	mContext->printGraph();

//	triggerApply();
	triggerApply2();
}

void ParamTestApp::setupBasic()
{
	mGen->connect( mGain )->connect( mContext->getTarget() );
	mGen->start();
}

void ParamTestApp::setupFilter()
{
	mGen->connect( mLowPass )->connect( mGain )->connect( mPan )->connect( mContext->getTarget() );
	mGen->start();
}

void ParamTestApp::triggerApply()
{
	// (a): ramp volume to 0.7 of 0.2 seconds
//	mGain->getGainParam()->rampTo( 0.7f, 0.2f );

	mGen->getParamFreq()->rampTo( 220, 440, 1 );
//	mGen->getParamFreq()->rampTo( 220, 440, 1, Param::Options().delay( 0.5f ) );

	// PSEUDO CODE: possible syntax where context keeps references to Params, calling updateValueArray() (or just process() ?) on them each block:
	// - problem I have with this right now is that its alot more syntax for the common case (see: (a)) of ramping up volume
//	Context::master()->timeline()->apply( mGen->getParamFreq(), 220, 440, 1 );

	LOG_V << "num events: " << mGen->getParamFreq()->getNumEvents() << endl;
}

// 2 events - first apply the ramp, blowing away anything else, then append another event to happen after that
void ParamTestApp::triggerApply2()
{
	mGen->getParamFreq()->rampTo( 220, 1440, 1 );
	mGen->getParamFreq()->appendTo( 369.994f, 1 ); // F#4

	LOG_V << "num events: " << mGen->getParamFreq()->getNumEvents() << endl;
}

// append an event with random frequency and duration 1 second, allowing them to build up. new events begin from the end of the last event
void ParamTestApp::triggerAppend()
{
	mGen->getParamFreq()->appendTo( randFloat( 50, 800 ), 1.0f );

	LOG_V << "num events: " << mGen->getParamFreq()->getNumEvents() << endl;
}

// make a ramp after a 1 second delay
void ParamTestApp::triggerDelay()
{
	mGen->getParamFreq()->rampTo( 50, 440, 1, Param::Options().delay( 1 ) );
	LOG_V << "num events: " << mGen->getParamFreq()->getNumEvents() << endl;
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
//		mGain->getParamGain()->rampTo( mGainSlider.mValueScaled );
		mGain->getParamGain()->rampTo( mGainSlider.mValueScaled, 0.15f );
	}
	if( mPanSlider.hitTest( pos ) )
		mPan->setPos( mPanSlider.mValueScaled );
	if( mGenFreqSlider.hitTest( pos ) ) {
//		mGen->setFreq( mGenFreqSlider.mValueScaled );
//		mGen->getParamFreq()->rampTo( mGenFreqSlider.mValueScaled, 0.3f );
		mGen->getParamFreq()->rampTo( mGenFreqSlider.mValueScaled, 0.3f, Param::Options().rampFn( &rampExpo ) );
	}
	if( mLowPassFreqSlider.hitTest( pos ) )
		mLowPass->setCutoffFreq( mLowPassFreqSlider.mValueScaled );
}

void ParamTestApp::processTap( Vec2i pos )
{
	size_t selectorIndex = mTestSelector.mCurrentSectionIndex;

	if( mPlayButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );
	else if( mApplyButton.hitTest( pos ) )
		triggerApply();
	else if( mApplyAppendButton.hitTest( pos ) )
		triggerApply2();
	else if( mAppendButton.hitTest( pos ) )
		triggerAppend();
	else if( mDelayButton.hitTest( pos ) )
		triggerDelay();
	else if( mTestSelector.hitTest( pos ) && selectorIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V << "selected: " << currentTest << endl;

		bool enabled = mContext->isEnabled();
		mContext->stop();

		mContext->disconnectAllNodes();

		if( currentTest == "basic" )
			setupBasic();
		if( currentTest == "filter" )
			setupFilter();

		mContext->setEnabled( enabled );
		mContext->printGraph();
	}
	else
		processDrag( pos );
}

void ParamTestApp::keyDown( KeyEvent event )
{
	if( event.getCode() == KeyEvent::KEY_e )
		LOG_V << "mGen freq events: " << mGen->getParamFreq()->getNumEvents() << endl;
}

void ParamTestApp::draw()
{
	gl::clear();
	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( ParamTestApp, RendererGl )
