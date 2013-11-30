#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "cinder/audio2/audio.h"
#include "cinder/audio2/NodeSource.h"
#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/NodeFilter.h"
#include "cinder/audio2/CinderAssert.h"
#include "cinder/audio2/Debug.h"

#include "../../common/AudioTestGui.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class EffectNodeTestApp : public AppNative {
  public:
	void setup();
	void draw();

	void setupOne();
	void setupForceStereo();
	void setupDownMix();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	Context*				mContext;
	NodeSourceRef			mGen;
	NodeGainRef				mGain;
	NodePan2dRef			mPan;
	NodeFilterLowPassRef	mLowPass;
//	shared_ptr<NodeFilterHighPass>	mLowPass;


	vector<TestWidget *>	mWidgets;
	Button					mPlayButton;
	VSelector				mTestSelector;
	HSlider					mGainSlider, mPanSlider, mLowPassFreqSlider, mFilterParam2Slider;
};

void EffectNodeTestApp::setup()
{
	mContext = Context::master();

	mGain = mContext->makeNode( new NodeGain() );
	mGain->setValue( 0.6f );

	mPan = mContext->makeNode( new NodePan2d() );
	mGen = mContext->makeNode( new NodeGenNoise( Node::Format().autoEnable() ) );

	mLowPass = mContext->makeNode( new NodeFilterLowPass() );
//	mLowPass = mContext->makeNode( new NodeFilterHighPass() );

	setupOne();
//	setupForceStereo();
//	setupDownMix();

	setupUI();

	mContext->printGraph();
}

void EffectNodeTestApp::setupOne()
{
	mGen->connect( mLowPass )->connect( mGain )->connect( mPan )->connect( mContext->getTarget() );
}

void EffectNodeTestApp::setupForceStereo()
{
	mGen->connect( mLowPass )->connect( mGain )->connect( mPan )->connect( mContext->getTarget() );
}

void EffectNodeTestApp::setupDownMix()
{
	auto mono = mContext->makeNode( new NodeGain( Node::Format().channels( 1 ) ) );
	mGen->connect( mLowPass )->connect( mGain )->connect( mPan )->connect( mono )->connect( mContext->getTarget() );
}

void EffectNodeTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.mBounds = Rectf( 0, 0, 200, 60 );
	mWidgets.push_back( &mPlayButton );

	mTestSelector.mSegments.push_back( "one" );
	mTestSelector.mSegments.push_back( "force stereo" );
	mTestSelector.mSegments.push_back( "down-mix" );
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
	mLowPassFreqSlider.mBounds = sliderRect;
	mLowPassFreqSlider.mTitle = "LowPass Freq";
	mLowPassFreqSlider.mMax = 1000.0f;
//	mLowPassFreqSlider.mMax = 10000.0f;
	mLowPassFreqSlider.set( mLowPass->getCutoffFreq() );
	mWidgets.push_back( &mLowPassFreqSlider );

	sliderRect += Vec2f( 0.0f, sliderRect.getHeight() + 10.0f );
	mFilterParam2Slider.mBounds = sliderRect;
	mFilterParam2Slider.mTitle = "filter resonance";
	mFilterParam2Slider.mMax = 50.0f;
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

void EffectNodeTestApp::processDrag( Vec2i pos )
{
	if( mGainSlider.hitTest( pos ) )
		mGain->setValue( mGainSlider.mValueScaled );
	if( mPanSlider.hitTest( pos ) )
		mPan->setPos( mPanSlider.mValueScaled );
	if( mLowPassFreqSlider.hitTest( pos ) )
		mLowPass->setCutoffFreq( mLowPassFreqSlider.mValueScaled );
	if( mFilterParam2Slider.hitTest( pos ) )
		mLowPass->setResonance( mFilterParam2Slider.mValueScaled );
}

void EffectNodeTestApp::processTap( Vec2i pos )
{
	if( mPlayButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );

	size_t currentIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V << "selected: " << currentTest << endl;

		bool enabled = mContext->isEnabled();
		mContext->stop();

		mContext->disconnectAllNodes();

		if( currentTest == "one" )
			setupOne();
		if( currentTest == "force stereo" )
			setupForceStereo();
		if( currentTest == "down-mix" )
			setupDownMix();

		mContext->setEnabled( enabled );
		mContext->printGraph();
	}
}

void EffectNodeTestApp::draw()
{
	gl::clear();
	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( EffectNodeTestApp, RendererGl )
