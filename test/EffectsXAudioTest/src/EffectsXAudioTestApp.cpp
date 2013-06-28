#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/audio.h"
#include "audio2/GeneratorNode.h"
#include "audio2/EffectNode.h"
#include "audio2/assert.h"
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
	void setupFilterDelay();
	void setupNativeThenGeneric();

	void setupUI();
	void processEvent( Vec2i pos );
	void updateLowpass();
	void updateEcho();

	ContextRef mContext;

	NodeRef mSource;
	shared_ptr<EffectXAudioXapo> mEffect, mEffect2;
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
	console() << "\t frames per block: " << device->getNumFramesPerBlock() << endl;

	auto output = Context::instance()->createOutput( device );
	mContext = Context::instance()->createContext();
	mContext->setRoot( output );

	auto noise = make_shared<UGenNode<NoiseGen> >();
	noise->getUGen().setAmp( 0.25f );
	noise->setAutoEnabled();
	//noise->getFormat().setNumChannels( 1 ); // force gen to be mono
	mSource = noise;

	//setupOne();
	//setupTwo(); // TODO: check this is working, sounds like maybe it isn't
	//setupFilter();
	setupFilterDelay();
	//setupNativeThenGeneric(); // TODO: not yet implemented, throws...

	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (before)" << endl;
	printGraph( mContext );

	mContext->initialize();

	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (after)" << endl;
	printGraph( mContext );

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
}

void EffectXAudioTestApp::setupOne()
{
	mEffect = make_shared<EffectXAudioXapo>( EffectXAudioXapo::XapoType::FXEQ );
	//mEffect->getFormat().setNumChannels( 2 ); // force effect to be stereo

	mSource->connect( mEffect )->connect( mContext->getRoot() );
}

void EffectXAudioTestApp::setupTwo()
{
	Node::Format format;
	//format.channels( 2 ); // force stereo

	mEffect = make_shared<EffectXAudioXapo>( EffectXAudioXapo::XapoType::FXEQ, format );
	mEffect2 = make_shared<EffectXAudioXapo>( EffectXAudioXapo::XapoType::FXEcho );

	mSource->connect( mEffect )->connect( mEffect2 )->connect( mContext->getRoot() );
}

void EffectXAudioTestApp::setupFilter()
{
	mFilterEffect = make_shared<EffectXAudioFilter>();

	mSource->connect( mFilterEffect )->connect( mContext->getRoot() );
}

void EffectXAudioTestApp::setupFilterDelay()
{
	mFilterEffect = make_shared<EffectXAudioFilter>();
	mEffect2 = make_shared<EffectXAudioXapo>( EffectXAudioXapo::XapoType::FXEcho );

	mSource->connect( mFilterEffect )->connect( mEffect2 )->connect( mContext->getRoot() );
}

void EffectXAudioTestApp::setupNativeThenGeneric()
{
	mEffect = make_shared<EffectXAudioXapo>( EffectXAudioXapo::XapoType::FXEQ );
	auto ringMod = make_shared<RingMod>();

	mSource->connect( mEffect )->connect( ringMod )->connect( mContext->getRoot() );
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
		mFilterParams.Frequency = XAudio2CutoffFrequencyToRadians( mLowpassCutoffSlider.valueScaled, mContext->getSampleRate() );
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
		mContext->setEnabled( ! mContext->isEnabled() );
}

void EffectXAudioTestApp::mouseDrag( MouseEvent event )
{
	processEvent( event.getPos() );
}

void EffectXAudioTestApp::touchesBegan( TouchEvent event )
{
	if( mPlayButton.hitTest( event.getTouches().front().getPos() ) )
		mContext->setEnabled( ! mContext->isEnabled() );
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
