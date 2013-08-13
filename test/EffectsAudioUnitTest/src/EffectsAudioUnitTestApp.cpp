#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/audio.h"
#include "audio2/GeneratorNode.h"
#include "audio2/CinderAssert.h"
#include "audio2/Debug.h"

#include "audio2/cocoa/ContextAudioUnit.h"

#include "Gui.h"

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace audio2;
using namespace audio2::cocoa;

struct TestConstGen : public UGen {
	TestConstGen( float val = 0.5f ) : UGen(), mVal( val )	{}

	using UGen::process;
	void process( float *channel, size_t count ) override {
		float val = mVal;
		for( size_t i = 0; i < count; i++ )
			channel[i] = val;
	}

private:
	float mVal;
};

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
	void initContext();

	ContextRef mContext;
	GeneratorNodeRef mSource;

	shared_ptr<EffectAudioUnit> mEffect, mEffect2;

	VSelector mTestSelector;
	Button mPlayButton;
	HSlider mLowpassCutoffSlider, mBandpassSlider;
};

void EffectsAudioUnitTestApp::setup()
{
	DeviceRef device = Device::getDefaultOutput();

	LOG_V << "device name: " << device->getName() << endl;
	console() << "\t input channels: " << device->getNumInputChannels() << endl;
	console() << "\t output channels: " << device->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << device->getSampleRate() << endl;
	console() << "\t block size: " << device->getNumFramesPerBlock() << endl;

	auto output = Context::instance()->createLineOut( device );
	mContext = Context::instance()->createContext();
	mContext->setRoot( output );


	auto noise = make_shared<UGenNode<NoiseGen> >();
	noise->setAutoEnabled();
	noise->getUGen().setAmp( 0.25f );
	//noise->getFormat().setNumChannels( 1 ); // force gen to be mono
	mSource = noise;

//	auto test = make_shared<UGenNode<TestConstGen> >();
//	test->setAutoEnabled();
//	//noise->getFormat().setNumChannels( 1 ); // force gen to be mono
//	mSource = test;

	setupOne();

	setupUI();
	initContext();
}

void EffectsAudioUnitTestApp::setupOne()
{
	mEffect = make_shared<EffectAudioUnit>( kAudioUnitSubType_LowPassFilter );
	mSource->connect( mEffect )->connect( mContext->getRoot() );

	mBandpassSlider.hidden = true;
}

void EffectsAudioUnitTestApp::setupTwo()
{
	mEffect = make_shared<EffectAudioUnit>( kAudioUnitSubType_LowPassFilter );
	mEffect2 = make_shared<EffectAudioUnit>( kAudioUnitSubType_BandPassFilter );

//	mEffect->getFormat().setNumChannels( 2 ); // force stereo

	mSource->connect( mEffect )->connect( mEffect2 )->connect( mContext->getRoot() );

	mBandpassSlider.hidden = false;
}

void EffectsAudioUnitTestApp::setupNativeThenGeneric()
{
	LOG_V << "TODO: implement test" << endl;
}

void EffectsAudioUnitTestApp::initContext()
{
	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (before)" << endl;
	printGraph( mContext );

	mContext->initialize();

	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (after)" << endl;
	printGraph( mContext );

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

	mTestSelector.segments = { "one", "two", "native -> generic" };
//	mTestSelector.bounds = Rectf( getWindowCenter().x + 100, 0.0f, getWindowWidth(), 160.0f );

//#if defined( CINDER_COCOA_TOUCH )
//	mPlayButton.bounds = Rectf( 0, 0, 120, 60 );
//	mPlayButton.textIsCentered = false;
//	mTestSelector.bounds = Rectf( getWindowWidth() - 190, 0.0f, getWindowWidth(), 160.0f );
//	mTestSelector.textIsCentered = false;
//#else
	mPlayButton.bounds = Rectf( 0, 0, 200, 60 );
	mTestSelector.bounds = Rectf( getWindowCenter().x + 100, 0.0f, getWindowWidth(), 160.0f );
//#endif

	float width = std::min( (float)getWindowWidth() - 20.0f,  440.0f );
	Rectf sliderRect( getWindowCenter().x - width / 2.0f, 200, getWindowCenter().x + width / 2.0f, 250 );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mLowpassCutoffSlider.bounds = sliderRect;
	mLowpassCutoffSlider.title = "Lowpass Cutoff";
	mLowpassCutoffSlider.max = 1500.0f;

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mBandpassSlider.bounds = sliderRect;
	mBandpassSlider.title = "Bandpass";
	mBandpassSlider.min = 100.0f;
	mBandpassSlider.max = 2000.0f;

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
		mEffect->setParameter( kLowPassParam_CutoffFrequency, mLowpassCutoffSlider.valueScaled );

	if( mEffect2 && mBandpassSlider.hitTest( pos ) )
		mEffect2->setParameter( kBandpassParam_CenterFrequency, mBandpassSlider.valueScaled );
}

void EffectsAudioUnitTestApp::processTap( Vec2i pos )
{
	if( mPlayButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );

	size_t currentIndex = mTestSelector.currentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.currentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V << "selected: " << currentTest << endl;

		bool running = mContext->isEnabled();
		mContext->uninitialize();

		// TODO: a 'disconnect all' method would be nice for this, although it probably wouldn't get used too much in practice

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
		initContext();

		if( running )
			mContext->start();
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
