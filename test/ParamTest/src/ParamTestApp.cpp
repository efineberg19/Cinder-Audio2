#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/audio.h"
#include "audio2/NodeSource.h"
#include "audio2/NodeEffect.h"
#include "audio2/NodeFilter.h"
#include "audio2/CinderAssert.h"
#include "audio2/Debug.h"

#include "Gui.h"

// TODO - account for multiple Param::Events
//			- need an AudioTimeline here?

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace ci::audio2;

class ParamTestApp : public AppNative {
  public:
	void setup();
	void draw();

	void setupBasic();
	void setupFilter();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	void triggerRamp();

	Context*				mContext;
	NodeGenRef				mGen;
	NodeGainRef				mGain;
	NodePan2dRef			mPan;
	NodeFilterLowPassRef	mLowPass;

	vector<TestWidget *>	mWidgets;
	Button					mPlayButton, mRampButton;
	VSelector				mTestSelector;
	HSlider					mGainSlider, mPanSlider, mLowPassFreqSlider, mGenFreqSlider;
};

void ParamTestApp::setup()
{
	mContext = Context::master();

	mGain = mContext->makeNode( new NodeGain() );
	mGain->setGain( 0.8 );

	mPan = mContext->makeNode( new NodePan2d() );

//	mGen = mContext->makeNode( new NodeGenTriangle() );
	mGen = mContext->makeNode( new NodeGenSine() );
//	mGen = mContext->makeNode( new NodeGenPhasor() );


	// TODO: this isn't possible now because rampTo requires the Context to already be set.
	// - needs the current number of processed seconds.  could set it to 0 if no context, but this may be a bit too hacky.
	mGen->setFreq( 0 );
//	mGen->getParamFreq()->rampTo( 100, 0.5f );

	mLowPass = mContext->makeNode( new NodeFilterLowPass() );

	setupBasic();

	setupUI();

	mContext->printGraph();

	triggerRamp();
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

void ParamTestApp::triggerRamp()
{
//	mGain->getGainParam()->setValue( 0.0f );
//	mGain->getGainParam()->rampTo( 0.8f, 0.5f, 1.0f );
//	mGain->getGainParam()->rampTo( 1.0f, 1.0f );

//	mGain->getGainParam()->rampTo( 0.7f, 0.2f );

//	mGen->getParamFreq()->rampTo( randFloat( 60, 600 ), 0.5f, 0.0f );
	mGen->getParamFreq()->rampTo( 100, 0.03f );
}

void ParamTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.mBounds = Rectf( 0, 0, 200, 60 );
	mWidgets.push_back( &mPlayButton );

	mRampButton = Button( false, "ramp" );
	mRampButton.mBounds = mPlayButton.mBounds + Vec2f( mPlayButton.mBounds.x2 + 10.0f, 0.0f );
	mWidgets.push_back( &mRampButton );

	mTestSelector.mSegments.push_back( "basic" );
	mTestSelector.mSegments.push_back( "filter" );
	mTestSelector.mBounds = Rectf( getWindowWidth() * 0.67f, 0.0f, getWindowWidth(), 160.0f );
	mWidgets.push_back( &mTestSelector );

	float width = std::min( (float)getWindowWidth() - 20.0f,  440.0f );
	Rectf sliderRect( getWindowCenter().x - width / 2.0f, 200, getWindowCenter().x + width / 2.0f, 250 );
	mGainSlider.mBounds = sliderRect;
	mGainSlider.mTitle = "Gain";
	mGainSlider.set( mGain->getGain() );
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
//		mGain->setGain( mGainSlider.mValueScaled );
//		mGain->getGainParam()->rampTo( mGainSlider.mValueScaled );
		mGain->getGainParam()->rampTo( mGainSlider.mValueScaled, 0.15f );
	}
	if( mPanSlider.hitTest( pos ) )
		mPan->setPos( mPanSlider.mValueScaled );
	if( mGenFreqSlider.hitTest( pos ) ) {
//		mGen->setFreq( mGenFreqSlider.mValueScaled );
//		mGen->getParamFreq()->rampTo( mGenFreqSlider.mValueScaled, 0.3f );
		mGen->getParamFreq()->rampTo( mGenFreqSlider.mValueScaled, 0.3f, &rampExpo );
	}
	if( mLowPassFreqSlider.hitTest( pos ) )
		mLowPass->setCutoffFreq( mLowPassFreqSlider.mValueScaled );
}

void ParamTestApp::processTap( Vec2i pos )
{
	size_t selectorIndex = mTestSelector.mCurrentSectionIndex;

	if( mPlayButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );
	else if( mRampButton.hitTest( pos ) )
		triggerRamp();
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

void ParamTestApp::draw()
{
	gl::clear();
	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( ParamTestApp, RendererGl )
