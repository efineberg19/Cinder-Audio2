#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/audio.h"
#include "audio2/GeneratorNode.h"
#include "audio2/EffectNode.h"
#include "audio2/CinderAssert.h"
#include "audio2/Debug.h"

#include "Gui.h"

// TODO: make some native effects like EffectNativeReverb, EffectNativeDelay, and replace those below

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace audio2;

class EffectNodeTestApp : public AppNative {
  public:
	void setup();
	void draw();

	void setupOne();
	void setupForceStereo();
	void setupDownMix();
	void initContext();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	ContextRef mContext;
	GeneratorNodeRef mGen;
	GainNodeRef mGain;
	shared_ptr<RingMod> mRingMod;

	vector<TestWidget *> mWidgets;
	Button mPlayButton;
	VSelector mTestSelector;
	HSlider mGainSlider, mRingModFreqSlider;
};

void EffectNodeTestApp::setup()
{
	mContext = Context::instance()->createContext();

	mGain = mContext->makeNode( new GainNode() );
	mGain->setGain( 0.6f );

//	auto noise = mContext->makeNode( new UGenNode<NoiseGen>() );
//	noise->getUGen().setAmp( 0.25f );
//	mGen = noise;

	auto sine = mContext->makeNode( new UGenNode<SineGen>() );
	sine->getUGen().setAmp( 1.0f );
	sine->getUGen().setFreq( 440.0f );
	mGen = sine;
	mGen->setAutoEnabled();

	setupOne();
//	setupForceStereo();
//	setupDownMix();

	initContext();
	setupUI();
}

void EffectNodeTestApp::setupOne()
{
	mRingMod = mContext->makeNode( new RingMod() );
	mRingMod->mSineGen.setFreq( 20.0f );
	mGen->connect( mRingMod )->connect( mGain )->connect( mContext->getRoot() );
}

// TODO NEXT: GainNode should be flexible in channel counts
//	- it should accomodate any inpute channel count and always operates in-place
void EffectNodeTestApp::setupForceStereo()
{
	mRingMod = mContext->makeNode( new RingMod( Node::Format().channels( 2 ) ) );
	mRingMod->mSineGen.setFreq( 20.0f );
	mGen->connect( mRingMod )->connect( mGain )->connect( mContext->getRoot() );
}

void EffectNodeTestApp::setupDownMix()
{
	mRingMod = mContext->makeNode( new RingMod( Node::Format().channels( 2 ) ) );
	mRingMod->mSineGen.setFreq( 20.0f );

	auto monoPassThru = mContext->makeNode( new Node( Node::Format().channels( 1 ) ) );
	mGen->connect( mRingMod )->connect( mGain )->connect( monoPassThru )->connect( mContext->getRoot() );
}

void EffectNodeTestApp::initContext()
{
	LOG_V << "------------------------- Graph configuration: -------------------------" << endl;
	printGraph( mContext );

	mContext->initialize();
}

void EffectNodeTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.bounds = Rectf( 0, 0, 200, 60 );
	mWidgets.push_back( &mPlayButton );

	mTestSelector.segments.push_back( "one" );
	mTestSelector.segments.push_back( "force stereo" );
	mTestSelector.segments.push_back( "down-mix" );
	mTestSelector.bounds = Rectf( getWindowWidth() * 0.67f, 0.0f, getWindowWidth(), 160.0f );
	mWidgets.push_back( &mTestSelector );

	float width = std::min( (float)getWindowWidth() - 20.0f,  440.0f );
	Rectf sliderRect( getWindowCenter().x - width / 2.0f, 200, getWindowCenter().x + width / 2.0f, 250 );
	mGainSlider.bounds = sliderRect;
	mGainSlider.title = "Gain";
	mGainSlider.max = 1.0f;
	mGainSlider.set( mGain->getGain() );
	mWidgets.push_back( &mGainSlider );

	sliderRect += Vec2f( 0.0f, sliderRect.getHeight() + 10.0f );
	mRingModFreqSlider.bounds = sliderRect;
	mRingModFreqSlider.title = "RingMod Frequency";
	mRingModFreqSlider.max = 500.0f;
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
		mGain->setGain( mGainSlider.valueScaled );
	if( mRingModFreqSlider.hitTest( pos ) )
		mRingMod->mSineGen.setFreq( mRingModFreqSlider.valueScaled );
}

void EffectNodeTestApp::processTap( Vec2i pos )
{
	if( mPlayButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );

	size_t currentIndex = mTestSelector.currentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.currentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V << "selected: " << currentTest << endl;

		bool running = mContext->isEnabled();
		mContext->uninitialize();

		mContext->disconnectAllNodes();

		if( currentTest == "one" )
			setupOne();
		if( currentTest == "force stereo" )
			setupForceStereo();
		if( currentTest == "down-mix" )
			setupDownMix();
		initContext();

		if( running )
			mContext->start();
	}
}

void EffectNodeTestApp::draw()
{
	gl::clear();
	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( EffectNodeTestApp, RendererGl )
