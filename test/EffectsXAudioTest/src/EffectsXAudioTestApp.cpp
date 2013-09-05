#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/audio.h"
#include "audio2/GeneratorNode.h"
#include "audio2/EffectNode.h"
#include "audio2/CinderAssert.h"
#include "audio2/Debug.h"

#include "audio2/msw/ContextXAudio.h"

#include "Gui.h"

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace audio2;
using namespace audio2::msw;

class EffectXAudioTestApp : public AppNative {
public:
	void setup();
	void draw();

	void setupOne();
	void setupTwo();
	void setupFilter();
	void setupFilterThenDelay();
	void setupNativeThenGeneric();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );
	void initContext();
	void updateLowpass();
	void updateEcho();

	ContextRef mContext;

	NodeRef mSource;
	shared_ptr<EffectXAudioXapo> mEffect, mEffect2;
	FXEQ_PARAMETERS mEQParams;
	FXECHO_PARAMETERS mEchoParams;

	shared_ptr<EffectXAudioFilter> mFilterEffect;
	XAUDIO2_FILTER_PARAMETERS mFilterParams;

	vector<TestWidget *> mWidgets;
	Button mPlayButton;
	VSelector mTestSelector;
	HSlider mLowpassCutoffSlider, mEchoDelaySlider;
};

void EffectXAudioTestApp::setup()
{
	mContext = Context::instance()->createContext();

	auto noise = mContext->makeNode( new UGenNode<NoiseGen>() );
	noise->getUGen().setAmp( 0.25f );
	noise->setAutoEnabled();
	//noise->getFormat().setNumChannels( 1 ); // force gen to be mono
	mSource = noise;

	setupOne();
	setupUI();

	if( mEffect ) {
		mEffect->getParams( &mEQParams );

		// reset so it's like a lowpass
		mEQParams.Gain0 = FXEQ_MAX_GAIN;
		mEQParams.Gain1 = FXEQ_MIN_GAIN;
		mEQParams.Gain2 = FXEQ_MIN_GAIN;
		mEQParams.Gain3 = FXEQ_MIN_GAIN;

		mEffect->setParams( mEQParams );

		mLowpassCutoffSlider.set( mEQParams.FrequencyCenter0 );
	}

	if( mEffect2 ) {
		mEffect2->getParams( &mEchoParams );
		mEchoDelaySlider.set( mEchoParams.Delay );
	}

	if( mFilterEffect ) {
		mFilterEffect->getParams( &mFilterParams );
		mFilterParams.Type = LowPassFilter;
		mFilterEffect->setParams( mFilterParams );

		float cutoff = XAudio2FrequencyRatioToSemitones( mFilterParams.Frequency );
		mLowpassCutoffSlider.set( cutoff );
	}

	LOG_V << "------------------------- Graph configuration: -------------------------" << endl;
	printGraph( mContext );
}

void EffectXAudioTestApp::setupOne()
{
	mEffect =  mContext->makeNode( new EffectXAudioXapo( EffectXAudioXapo::XapoType::FXEQ ) );
	//mEffect->getFormat().setNumChannels( 2 ); // force effect to be stereo

	mSource->connect( mEffect )->connect( mContext->getRoot() );
}

void EffectXAudioTestApp::setupTwo()
{
	Node::Format format;
	//format.channels( 2 ); // force stereo

	mEffect = mContext->makeNode( new EffectXAudioXapo( EffectXAudioXapo::XapoType::FXEQ, format ) );
	mEffect2 = mContext->makeNode( new EffectXAudioXapo( EffectXAudioXapo::XapoType::FXEcho ) );

	mSource->connect( mEffect )->connect( mEffect2 )->connect( mContext->getRoot() );
}

void EffectXAudioTestApp::setupFilter()
{
	mFilterEffect = mContext->makeNode( new EffectXAudioFilter() );

	mSource->connect( mFilterEffect )->connect( mContext->getRoot() );
}

void EffectXAudioTestApp::setupFilterThenDelay()
{
	mFilterEffect = mContext->makeNode( new EffectXAudioFilter() );
	mEffect2 =  mContext->makeNode( new EffectXAudioXapo( EffectXAudioXapo::XapoType::FXEcho ) );

	mSource->connect( mFilterEffect )->connect( mEffect2 )->connect( mContext->getRoot() );
}

void EffectXAudioTestApp::setupNativeThenGeneric()
{
	// TODO: catch exception

	mEffect = mContext->makeNode( new EffectXAudioXapo( EffectXAudioXapo::XapoType::FXEQ ) );
	auto ringMod =  mContext->makeNode( new RingMod() );

	mSource->connect( mEffect )->connect( ringMod )->connect( mContext->getRoot() );
}

void EffectXAudioTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.mBounds = Rectf( 0, 0, 200, 60 );
	mWidgets.push_back( &mPlayButton );

	mTestSelector.mSegments.push_back( "one" );
	mTestSelector.mSegments.push_back( "two" );
	mTestSelector.mSegments.push_back( "filter" );
	mTestSelector.mSegments.push_back( "filter -> delay" );
	mTestSelector.mSegments.push_back( "native -> generic" );
	mTestSelector.mBounds = Rectf( (float)getWindowWidth() * 0.67f, 0.0f, (float)getWindowWidth(), 160.0f );
	mWidgets.push_back( &mTestSelector );

	float width = std::min( (float)getWindowWidth() - 20.0f,  440.0f );
	Rectf sliderRect( getWindowCenter().x - width / 2.0f, 200, getWindowCenter().x + width / 2.0f, 250 );
	mLowpassCutoffSlider.mBounds = sliderRect;
	mLowpassCutoffSlider.mTitle = "Lowpass Cutoff";
	mLowpassCutoffSlider.mMax = 1500.0f;
	mWidgets.push_back( &mLowpassCutoffSlider );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mEchoDelaySlider.mBounds = sliderRect;
	mEchoDelaySlider.mTitle = "Echo Delay";
	mEchoDelaySlider.mMin = 1.0f;
	mEchoDelaySlider.mMax = 2000.0f;
	mWidgets.push_back( &mEchoDelaySlider );

	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalMouseDrag().connect( [this] ( MouseEvent &event ) { processDrag( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );
	getWindow()->getSignalTouchesMoved().connect( [this] ( TouchEvent &event ) {
		for( const TouchEvent::Touch &touch : getActiveTouches() )
			processDrag( touch.getPos() );
	} );

	gl::enableAlphaBlending();
}

void EffectXAudioTestApp::updateLowpass()
{
	if( mFilterEffect ) {
		mFilterParams.Frequency = XAudio2CutoffFrequencyToRadians( mLowpassCutoffSlider.mValueScaled, mContext->getSampleRate() );
		mFilterEffect->setParams( mFilterParams );
	}
}

void EffectXAudioTestApp::updateEcho()
{
	if( mEffect2 ) {
		mEchoParams.Delay = std::max( FXECHO_MIN_DELAY, mEchoDelaySlider.mValueScaled ); // seems like the effect shuts off if this is set to 0... probably worth protecting against it
		mEffect2->setParams( mEchoParams );
	}
}

void EffectXAudioTestApp::processDrag( Vec2i pos )
{
	if( mLowpassCutoffSlider.hitTest( pos ) )
		updateLowpass();
	
	if( mEchoDelaySlider.hitTest( pos ) )
		updateEcho();
}

void EffectXAudioTestApp::processTap( Vec2i pos )
{
	if( mPlayButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );

	size_t currentIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V << "selected: " << currentTest << endl;

		bool enabled = mContext->isEnabled();
		mContext->disconnectAllNodes();

		if( currentTest == "one" )
			setupOne();
		if( currentTest == "two" )
			setupTwo();
		if( currentTest == "filter" )
			setupFilter();
		if( currentTest == "filter -> delay" )
			setupFilterThenDelay();
		if( currentTest == "native -> generic" )
			setupNativeThenGeneric();

		mContext->setEnabled( enabled );
	}
}

void EffectXAudioTestApp::draw()
{
	gl::clear();
	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( EffectXAudioTestApp, RendererGl )
