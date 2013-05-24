#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/Device.h"
#include "audio2/Graph.h"
#include "audio2/Engine.h"
#include "audio2/UGen.h"
#include "audio2/audio.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

#if defined( CINDER_COCOA )
#include "audio2/GraphAudioUnit.h"
#elif defined( CINDER_MSW )
#include "audio2/GraphXAudio.h"
#endif

#include "Gui.h"

// TODO NEXT: FilterEffectXAudio

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace audio2;

template <typename UGenT>
struct UGenNode : public Producer {
	UGenNode()	{
		mTag = "UGenNode";
 		mFormat.setWantsDefaultFormatFromParent();
	}

	virtual void render( BufferT *buffer ) override {
		mGen.render( buffer );
	}

	UGenT mGen;
};

class EffectTestApp : public AppNative {
  public:
	void prepareSettings( Settings *settings );
	void setup();
	void keyDown( KeyEvent event );
	void touchesBegan( TouchEvent event ) override;
	void touchesMoved( TouchEvent event ) override;
	void mouseDown( MouseEvent event ) override;
	void mouseDrag( MouseEvent event ) override;
	void update();
	void draw();

	void setupOne();
	void setupTwo();
	void toggleGraph();

	void setupUI();
	void processEvent( Vec2i pos );
	void updateLowpass();
	void updateEcho();

	GraphRef mGraph;

#if defined( CINDER_COCOA )
	shared_ptr<EffectAudioUnit> mEffect, mEffect2;
#elif defined( CINDER_MSW )
	shared_ptr<EffectXAudio> mEffect, mEffect2;
	FXEQ_PARAMETERS mEQParams;
	FXECHO_PARAMETERS mEchoParams;
#endif
	Button mPlayButton;
	HSlider mNoisePanSlider, mFreqPanSlider, mLowpassCutoffSlider, mBandPassCenterSlider;
};

void EffectTestApp::prepareSettings( Settings *settings )
{
//	settings->enableMultiTouch();
}

void EffectTestApp::setup()
{
	DeviceRef device = Device::getDefaultOutput();

	LOG_V << "device name: " << device->getName() << endl;
	console() << "\t input channels: " << device->getNumInputChannels() << endl;
	console() << "\t output channels: " << device->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << device->getSampleRate() << endl;
	console() << "\t block size: " << device->getBlockSize() << endl;

	auto output = Engine::instance()->createOutput( device );
	mGraph = Engine::instance()->createGraph();
	mGraph->setOutput( output );

	//setupOne();
	setupTwo();

	mGraph->initialize();

	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration:" << endl;
	printGraph( mGraph );
	
	setupUI();

#if defined( CINDER_COCOA )
	if( mEffect ) {
		mEffect->setParameter( kLowPassParam_CutoffFrequency, 500 );
		mLowpassCutoffSlider.set( 500 );
	}

	if( mEffect2 ) {
		mEffect2->setParameter( kBandpassParam_CenterFrequency, 1000 );
		mEffect2->setParameter( kBandpassParam_Bandwidth, 1200 );
		mBandPassCenterSlider.set( 1000 );
	}
#elif defined( CINDER_MSW )
	mEffect->getParams( &mEQParams, sizeof( mEQParams ) );

	// reset so it's like a lowpass
	mEQParams.Gain0 = FXEQ_MAX_GAIN;
	mEQParams.Gain1 = FXEQ_MIN_GAIN;
	mEQParams.Gain2 = FXEQ_MIN_GAIN;
	mEQParams.Gain3 = FXEQ_MIN_GAIN;

	mEffect->setParams( &mEQParams, sizeof( mEQParams ) );

	mLowpassCutoffSlider.set( mEQParams.FrequencyCenter0 );

	if( mEffect2 ) {
		mEffect2->getParams( &mEchoParams, sizeof( mEchoParams ) );
		mEffect2->setParams( &mEchoParams, sizeof( mEchoParams ) );
	}
#endif
}

void EffectTestApp::setupOne()
{
	auto noise = make_shared<UGenNode<NoiseGen> >();
	noise->mGen.setAmp( 0.25f );
	//noise->getFormat().setNumChannels( 1 ); // force gen to be mono

#if defined( CINDER_COCOA )
	mEffect = make_shared<EffectAudioUnit>( kAudioUnitSubType_LowPassFilter );
#else
	mEffect = make_shared<EffectXAudio>( EffectXAudio::XapoType::FXEQ );
#endif
	//mEffect->getFormat().setNumChannels( 2 ); // force effect to be stereo

	mEffect->connect( noise );
	mGraph->getOutput()->connect( mEffect );
}

void EffectTestApp::setupTwo()
{
	auto noise = make_shared<UGenNode<NoiseGen> >();
	noise->mGen.setAmp( 0.25f );
	//noise->getFormat().setNumChannels( 1 ); // force mono

#if defined( CINDER_COCOA )
	mEffect = make_shared<EffectAudioUnit>( kAudioUnitSubType_LowPassFilter );
	mEffect2 = make_shared<EffectAudioUnit>( kAudioUnitSubType_BandPassFilter );
#else
	mEffect = make_shared<EffectXAudio>( EffectXAudio::XapoType::FXEQ );
	mEffect2 = make_shared<EffectXAudio>( EffectXAudio::XapoType::FXEcho );
#endif

	mEffect->getFormat().setNumChannels( 2 ); // force stereo

	mEffect->connect( noise );
	mEffect2->connect( mEffect );
	mGraph->getOutput()->connect( mEffect2 );
}

void EffectTestApp::toggleGraph()
{
	if( ! mGraph->isRunning() )
		mGraph->start();
	else
		mGraph->stop();
}

void EffectTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.bounds = Rectf( 0, 0, 200, 60 );

	float width = std::min( (float)getWindowWidth() - 20.0f,  440.0f );
	Rectf sliderRect( getWindowCenter().x - width / 2.0f, 200, getWindowCenter().x + width / 2.0f, 250 );
	mNoisePanSlider.bounds = sliderRect;
	mNoisePanSlider.title = "Pan (Noise)";
	mNoisePanSlider.min = -1.0f;
	mNoisePanSlider.max = 1.0f;

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mFreqPanSlider.bounds = sliderRect;
	mFreqPanSlider.title = "Pan (Freq)";
	mFreqPanSlider.min = -1.0f;
	mFreqPanSlider.max = 1.0f;

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mLowpassCutoffSlider.bounds = sliderRect;
	mLowpassCutoffSlider.title = "Lowpass Cutoff (Noise)";
	mLowpassCutoffSlider.max = 1500.0f;

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mBandPassCenterSlider.bounds = sliderRect;
	mBandPassCenterSlider.title = "Bandpass Center (Noise)";
	mBandPassCenterSlider.min = 500.0f;
	mBandPassCenterSlider.max = 2500.0f;

	gl::enableAlphaBlending();
}

void EffectTestApp::keyDown( KeyEvent event )
{
}

void EffectTestApp::processEvent( Vec2i pos )
{
	if( mLowpassCutoffSlider.hitTest( pos ) )
		updateLowpass();
	
	if( mBandPassCenterSlider.hitTest( pos ) )
		updateEcho();
}

void EffectTestApp::updateLowpass()
{
	if( mEffect ) {
#if defined( CINDER_COCOA )
		mEffect->setParameter( kLowPassParam_CutoffFrequency, mLowpassCutoffSlider.valueScaled );
#elif defined( CINDER_MSW )
		mEQParams.FrequencyCenter0 = std::max( FXEQ_MIN_FREQUENCY_CENTER, mLowpassCutoffSlider.valueScaled ); // seems like the effect shuts off if this is set to 0... probably worth protecting against it
		mEffect->setParams( &mEQParams, sizeof( mEQParams ) );
#endif
	}
}

void EffectTestApp::updateEcho()
{

}

void EffectTestApp::mouseDown( MouseEvent event )
{
	if( mPlayButton.hitTest( event.getPos() ) )
		toggleGraph();
}

void EffectTestApp::mouseDrag( MouseEvent event )
{
	processEvent( event.getPos() );
}

void EffectTestApp::touchesBegan( TouchEvent event )
{
	if( mPlayButton.hitTest( event.getTouches().front().getPos() ) )
		toggleGraph();
}

void EffectTestApp::touchesMoved( TouchEvent event )
{
	for( const TouchEvent::Touch &touch : getActiveTouches() ) {
		processEvent( touch.getPos() );
	}
}

void EffectTestApp::update()
{
}

void EffectTestApp::draw()
{
	gl::clear();

	mPlayButton.draw();
	mNoisePanSlider.draw();
	mFreqPanSlider.draw();
	mLowpassCutoffSlider.draw();
	mBandPassCenterSlider.draw();
}

CINDER_APP_NATIVE( EffectTestApp, RendererGl )
