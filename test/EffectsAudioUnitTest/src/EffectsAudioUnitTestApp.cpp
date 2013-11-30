#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "cinder/audio2/audio.h"
#include "cinder/audio2/NodeSource.h"
#include "cinder/audio2/CinderAssert.h"
#include "cinder/audio2/Debug.h"

#include "cinder/audio2/cocoa/ContextAudioUnit.h"

#include "../../common/AudioTestGui.h"

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace audio2::cocoa;

class EffectsAudioUnitTestApp : public AppNative {
  public:
	void setup();
	void update();
	void draw();

	void setupOne();
	void setupTwo();
	void setupNativeThenGeneric();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );
	void initParams();

	Context* mContext;
	NodeSourceRef mSource;

	shared_ptr<NodeEffectAudioUnit> mEffect, mEffect2;

	VSelector mTestSelector;
	Button mPlayButton;
	HSlider mLowpassCutoffSlider, mBandpassSlider;
};

void EffectsAudioUnitTestApp::setup()
{
	mContext = Context::master();

	auto noise = mContext->makeNode( new NodeGenNoise() );
	noise->setAutoEnabled();
	noise->getGen().setAmp( 0.25f );
	//noise->getFormat().setNumChannels( 1 ); // force gen to be mono
	mSource = noise;

	setupOne();

	setupUI();
	initParams();

	mContext->printGraph();
}

void EffectsAudioUnitTestApp::setupOne()
{
	mEffect = mContext->makeNode( new NodeEffectAudioUnit( kAudioUnitSubType_LowPassFilter ) );
	mSource->connect( mEffect )->connect( mContext->getTarget() );

	mBandpassSlider.mHidden = true;
}

void EffectsAudioUnitTestApp::setupTwo()
{
	mEffect = mContext->makeNode( new NodeEffectAudioUnit( kAudioUnitSubType_LowPassFilter ) );
	mEffect2 = mContext->makeNode( new NodeEffectAudioUnit( kAudioUnitSubType_BandPassFilter ) );

//	mEffect->getFormat().setNumChannels( 2 ); // force stereo

	mSource->connect( mEffect )->connect( mEffect2 )->connect( mContext->getTarget() );

	mBandpassSlider.mHidden = false;
}

void EffectsAudioUnitTestApp::setupNativeThenGeneric()
{
	LOG_V << "TODO: implement test" << endl;
}

void EffectsAudioUnitTestApp::initParams()
{

	if( mEffect ) {
		mEffect->setParameter( kLowPassParam_CutoffFrequency, 500 );
		mLowpassCutoffSlider.set( 500 );
	}

	if( mEffect2 ) {
		mEffect2->setParameter( kBandpassParam_CenterFrequency, 1000 );
		mEffect2->setParameter( kBandpassParam_Bandwidth, 1200 );
		mBandpassSlider.set( 1000 );
	}
}

void EffectsAudioUnitTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
//	mPlayButton.bounds = Rectf( 0, 0, 200, 60 );

	mTestSelector.mSegments = { "one", "two", "native -> generic" };
//	mTestSelector.bounds = Rectf( getWindowCenter().x + 100, 0.0f, getWindowWidth(), 160.0f );

//#if defined( CINDER_COCOA_TOUCH )
//	mPlayButton.bounds = Rectf( 0, 0, 120, 60 );
//	mPlayButton.textIsCentered = false;
//	mTestSelector.bounds = Rectf( getWindowWidth() - 190, 0.0f, getWindowWidth(), 160.0f );
//	mTestSelector.textIsCentered = false;
//#else
	mPlayButton.mBounds = Rectf( 0, 0, 200, 60 );
	mTestSelector.mBounds = Rectf( getWindowCenter().x + 100, 0.0f, getWindowWidth(), 160.0f );
//#endif

	float width = std::min( (float)getWindowWidth() - 20.0f,  440.0f );
	Rectf sliderRect( getWindowCenter().x - width / 2.0f, 200, getWindowCenter().x + width / 2.0f, 250 );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mLowpassCutoffSlider.mBounds = sliderRect;
	mLowpassCutoffSlider.mTitle = "Lowpass Cutoff";
	mLowpassCutoffSlider.mMax = 1500.0f;

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mBandpassSlider.mBounds = sliderRect;
	mBandpassSlider.mTitle = "Bandpass";
	mBandpassSlider.mMin = 100.0f;
	mBandpassSlider.mMax = 2000.0f;

	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalMouseDrag().connect( [this] ( MouseEvent &event ) { processDrag( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );
	getWindow()->getSignalTouchesMoved().connect( [this] ( TouchEvent &event ) {
		for( const TouchEvent::Touch &touch : getActiveTouches() )
			processDrag( touch.getPos() );
	} );

	gl::enableAlphaBlending();
}

void EffectsAudioUnitTestApp::processDrag( Vec2i pos )
{
	if( mEffect && mLowpassCutoffSlider.hitTest( pos ) )
		mEffect->setParameter( kLowPassParam_CutoffFrequency, mLowpassCutoffSlider.mValueScaled );

	if( mEffect2 && mBandpassSlider.hitTest( pos ) )
		mEffect2->setParameter( kBandpassParam_CenterFrequency, mBandpassSlider.mValueScaled );
}

void EffectsAudioUnitTestApp::processTap( Vec2i pos )
{
	if( mPlayButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );

	size_t currentIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V << "selected: " << currentTest << endl;

		bool enabled = mContext->isEnabled();
		mContext->disconnectAllNodes();

		mSource->disconnect();
		mEffect->disconnect();
		if( mEffect2 )
			mEffect2->disconnect();

		if( currentTest == "one" )
			setupOne();
		if( currentTest == "two" )
			setupTwo();
		if( currentTest == "native -> generic" )
			setupNativeThenGeneric();
		initParams();

		mContext->setEnabled( enabled );
	}
}

void EffectsAudioUnitTestApp::update()
{
}

void EffectsAudioUnitTestApp::draw()
{
	gl::clear();

	mPlayButton.draw();

	if( mEffect )
		mLowpassCutoffSlider.draw();
	if( mEffect2 )
		mBandpassSlider.draw();

	mTestSelector.draw();
}

CINDER_APP_NATIVE( EffectsAudioUnitTestApp, RendererGl )
