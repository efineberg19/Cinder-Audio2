#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/Device.h"
#include "audio2/Graph.h"
#include "audio2/Engine.h"
#include "audio2/UGen.h"
#include "audio2/audio.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"
#include "audio2/GraphXAudio.h"

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

class EffectXAudioTestApp : public AppNative {
public:
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
	void setupFilter();

	void toggleGraph();

	void setupUI();
	void processEvent( Vec2i pos );
	void updateLowpass();
	void updateEcho();

	GraphRef mGraph;

	NodeRef mSource;
	shared_ptr<EffectXAudio> mEffect, mEffect2;
	FXEQ_PARAMETERS mEQParams;
	FXECHO_PARAMETERS mEchoParams;

	shared_ptr<EffectXAudioFilter> mFilterEffect;
	XAUDIO2_FILTER_PARAMETERS mFilterParams;

	Button mPlayButton;
	HSlider mNoisePanSlider, mFreqPanSlider, mLowpassCutoffSlider, mEchoDelaySlider;
};

void EffectXAudioTestApp::setup()
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

	auto noise = make_shared<UGenNode<NoiseGen> >();
	noise->mGen.setAmp( 0.25f );
	//noise->getFormat().setNumChannels( 1 ); // force gen to be mono
	mSource = noise;

	//setupOne();
	//setupTwo();
	setupFilter();

	mGraph->initialize();

	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration:" << endl;
	printGraph( mGraph );

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
		//mEffect2->getParams( &mEchoParams, sizeof( mEchoParams ) );
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
}

void EffectXAudioTestApp::setupOne()
{
	mEffect = make_shared<EffectXAudio>( EffectXAudio::XapoType::FXEQ );
	//mEffect->getFormat().setNumChannels( 2 ); // force effect to be stereo

	mEffect->connect( mSource );
	mGraph->getOutput()->connect( mEffect );
}

void EffectXAudioTestApp::setupTwo()
{
	mEffect = make_shared<EffectXAudio>( EffectXAudio::XapoType::FXEQ );
	mEffect2 = make_shared<EffectXAudio>( EffectXAudio::XapoType::FXEcho );

	mEffect->getFormat().setNumChannels( 2 ); // force stereo

	mEffect->connect( mSource );
	mEffect2->connect( mEffect );
	mGraph->getOutput()->connect( mEffect2 );
}

void EffectXAudioTestApp::setupFilter()
{
	mFilterEffect = make_shared<EffectXAudioFilter>();

	mFilterEffect->connect( mSource );
	mGraph->getOutput()->connect( mFilterEffect );
}

void EffectXAudioTestApp::toggleGraph()
{
	if( ! mGraph->isRunning() )
		mGraph->start();
	else
		mGraph->stop();
}

void EffectXAudioTestApp::setupUI()
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
	mLowpassCutoffSlider.title = "Lowpass Cutoff";
	mLowpassCutoffSlider.max = 1500.0f;

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mEchoDelaySlider.bounds = sliderRect;
	mEchoDelaySlider.title = "Echo Delay";
	mEchoDelaySlider.min = 1.0f;
	mEchoDelaySlider.max = 2000.0f;

	gl::enableAlphaBlending();
}

void EffectXAudioTestApp::keyDown( KeyEvent event )
{
}

void EffectXAudioTestApp::processEvent( Vec2i pos )
{
	if( mLowpassCutoffSlider.hitTest( pos ) )
		updateLowpass();

	if( mEchoDelaySlider.hitTest( pos ) )
		updateEcho();
}

void EffectXAudioTestApp::updateLowpass()
{
	if( mFilterEffect ) {
		mFilterParams.Frequency = XAudio2CutoffFrequencyToRadians( mLowpassCutoffSlider.valueScaled, mFilterEffect->getFormat().getSampleRate() );
		mFilterEffect->setParams( mFilterParams );
	}
}

void EffectXAudioTestApp::updateEcho()
{
	if( mEffect2 ) {
		mEchoParams.Delay = std::max( FXECHO_MIN_DELAY, mEchoDelaySlider.valueScaled ); // seems like the effect shuts off if this is set to 0... probably worth protecting against it
		mEffect2->setParams( mEchoParams );
	}
}

void EffectXAudioTestApp::mouseDown( MouseEvent event )
{
	if( mPlayButton.hitTest( event.getPos() ) )
		toggleGraph();
}

void EffectXAudioTestApp::mouseDrag( MouseEvent event )
{
	processEvent( event.getPos() );
}

void EffectXAudioTestApp::touchesBegan( TouchEvent event )
{
	if( mPlayButton.hitTest( event.getTouches().front().getPos() ) )
		toggleGraph();
}

void EffectXAudioTestApp::touchesMoved( TouchEvent event )
{
	for( const TouchEvent::Touch &touch : getActiveTouches() ) {
		processEvent( touch.getPos() );
	}
}

void EffectXAudioTestApp::update()
{
}

void EffectXAudioTestApp::draw()
{
	gl::clear();

	mPlayButton.draw();
	mNoisePanSlider.draw();
	mFreqPanSlider.draw();
	mLowpassCutoffSlider.draw();
	mEchoDelaySlider.draw();
}

CINDER_APP_NATIVE( EffectXAudioTestApp, RendererGl )
