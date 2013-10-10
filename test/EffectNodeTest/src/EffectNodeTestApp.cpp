#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/audio.h"
#include "audio2/NodeSource.h"
#include "audio2/NodeEffect.h"
#include "audio2/CinderAssert.h"
#include "audio2/Debug.h"

#include "Gui.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace ci::audio2;

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

	ContextRef				mContext;
	NodeSourceRef			mGen;
	NodeGainRef				mGain;
	NodePan2dRef			mPan;
	shared_ptr<RingMod>		mRingMod;

	vector<TestWidget *>	mWidgets;
	Button					mPlayButton;
	VSelector				mTestSelector;
	HSlider					mGainSlider, mPanSlider, mRingModFreqSlider;
};

void EffectNodeTestApp::setup()
{
	mContext = Context::create();

	mGain = mContext->makeNode( new NodeGain() );
	mGain->setGain( 0.6f );

	mPan = mContext->makeNode( new NodePan2d() );

//	auto noise = mContext->makeNode( new NodeGen<NoiseGen>() );
//	noise->getGen().setAmp( 0.25f );
//	mGen = noise;

	auto sine = mContext->makeNode( new NodeGen<SineGen>( Node::Format().autoEnable() ) );
	sine->getGen().setAmp( 1.0f );
	sine->getGen().setFreq( 440.0f );
	mGen = sine;

	setupOne();
//	setupForceStereo();
//	setupDownMix();

	setupUI();

	printGraph( mContext );
}

void EffectNodeTestApp::setupOne()
{
	mRingMod = mContext->makeNode( new RingMod() );
	mRingMod->mSineGen.setFreq( 20.0f );
	mGen->connect( mRingMod )->connect( mGain )->connect( mPan )->connect( mContext->getTarget() );
}

void EffectNodeTestApp::setupForceStereo()
{
	mRingMod = mContext->makeNode( new RingMod( Node::Format().channels( 2 ) ) );
	mRingMod->mSineGen.setFreq( 20.0f );
	mGen->connect( mRingMod )->connect( mGain )->connect( mPan )->connect( mContext->getTarget() );
}

void EffectNodeTestApp::setupDownMix()
{
	mRingMod = mContext->makeNode( new RingMod( Node::Format().channels( 2 ) ) );
	mRingMod->mSineGen.setFreq( 20.0f );

	auto monoPassThru = mContext->makeNode( new Node( Node::Format().channels( 1 ) ) );
	mGen->connect( mRingMod )->connect( mGain )->connect( mPan )->connect( monoPassThru )->connect( mContext->getTarget() );
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
	mGainSlider.set( mGain->getGain() );
	mWidgets.push_back( &mGainSlider );

	sliderRect += Vec2f( 0.0f, sliderRect.getHeight() + 10.0f );
	mPanSlider.mBounds = sliderRect;
	mPanSlider.mTitle = "Pan";
	mPanSlider.set( mPan->getPos() );
	mWidgets.push_back( &mPanSlider );

	sliderRect += Vec2f( 0.0f, sliderRect.getHeight() + 10.0f );
	mRingModFreqSlider.mBounds = sliderRect;
	mRingModFreqSlider.mTitle = "RingMod Frequency";
	mRingModFreqSlider.mMax = 500.0f;
	mRingModFreqSlider.set( mRingMod->mSineGen.getFreq() );
	mWidgets.push_back( &mRingModFreqSlider );

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
		mGain->setGain( mGainSlider.mValueScaled );
	if( mPanSlider.hitTest( pos ) )
		mPan->setPos( mPanSlider.mValueScaled );
	if( mRingModFreqSlider.hitTest( pos ) )
		mRingMod->mSineGen.setFreq( mRingModFreqSlider.mValueScaled );
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
	}
}

void EffectNodeTestApp::draw()
{
	gl::clear();
	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( EffectNodeTestApp, RendererGl )
