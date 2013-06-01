#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/GraphAudioUnit.h"
#include "audio2/Engine.h"
#include "audio2/GeneratorNode.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

#include "Gui.h"

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace audio2;

class EffectsAudioUnitTestApp : public AppNative {
  public:
	void setup();
	void mouseDown( MouseEvent event );	
	void update();
	void draw();
	void mouseDrag( MouseEvent event );
	void touchesBegan( TouchEvent event );
	void touchesMoved( TouchEvent event );


	void setupOne();
	void setupTwo();
	void setupNativeThenGeneric();

	void setupUI();
	void processEvent( Vec2i pos );
	void toggleGraph();

	GraphRef mGraph;
	NodeRef mSource; // ???: can this be GeneratorNodeRef?

	shared_ptr<EffectAudioUnit> mEffect, mEffect2;
	
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
	console() << "\t block size: " << device->getBlockSize() << endl;

	auto output = Engine::instance()->createOutput( device );
	mGraph = Engine::instance()->createGraph();
	mGraph->setRoot( output );


	auto noise = make_shared<UGenNode<NoiseGen> >();
	noise->mGen.setAmp( 0.25f );
	//noise->getFormat().setNumChannels( 1 ); // force gen to be mono
	mSource = noise;


//	setupOne();
	setupTwo();
//	setupNativeThenGeneric();

	
	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (before)" << endl;
	printGraph( mGraph );

	mGraph->initialize();

	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (after)" << endl;
	printGraph( mGraph );

	setupUI();

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

void EffectsAudioUnitTestApp::setupOne()
{
	mEffect = make_shared<EffectAudioUnit>( kAudioUnitSubType_LowPassFilter );

	mEffect->connect( mSource );
	mGraph->getRoot()->connect( mEffect );
}

void EffectsAudioUnitTestApp::setupTwo()
{
	mEffect = make_shared<EffectAudioUnit>( kAudioUnitSubType_LowPassFilter );
	mEffect2 = make_shared<EffectAudioUnit>( kAudioUnitSubType_BandPassFilter );

//	mEffect->getFormat().setNumChannels( 2 ); // force stereo

	mEffect->connect( mSource );
	mEffect2->connect( mEffect );
	mGraph->getRoot()->connect( mEffect2 );
}

void EffectsAudioUnitTestApp::setupNativeThenGeneric()
{
	
}

void EffectsAudioUnitTestApp::toggleGraph()
{
	if( ! mGraph->isRunning() )
		mGraph->start();
	else
		mGraph->stop();
}


void EffectsAudioUnitTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.bounds = Rectf( 0, 0, 200, 60 );

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

	gl::enableAlphaBlending();
}

void EffectsAudioUnitTestApp::processEvent( Vec2i pos )
{
	if( mEffect && mLowpassCutoffSlider.hitTest( pos ) )
		mEffect->setParameter( kLowPassParam_CutoffFrequency, mLowpassCutoffSlider.valueScaled );

	if( mEffect2 && mBandpassSlider.hitTest( pos ) )
		mEffect2->setParameter( kBandpassParam_CenterFrequency, mBandpassSlider.valueScaled );
}

void EffectsAudioUnitTestApp::mouseDown( MouseEvent event )
{
	if( mPlayButton.hitTest( event.getPos() ) )
		toggleGraph();
}

void EffectsAudioUnitTestApp::mouseDrag( MouseEvent event )
{
	processEvent( event.getPos() );
}

void EffectsAudioUnitTestApp::touchesBegan( TouchEvent event )
{
	if( mPlayButton.hitTest( event.getTouches().front().getPos() ) )
		toggleGraph();
}

void EffectsAudioUnitTestApp::touchesMoved( TouchEvent event )
{
	for( const TouchEvent::Touch &touch : getActiveTouches() ) {
		processEvent( touch.getPos() );
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
}

CINDER_APP_NATIVE( EffectsAudioUnitTestApp, RendererGl )
