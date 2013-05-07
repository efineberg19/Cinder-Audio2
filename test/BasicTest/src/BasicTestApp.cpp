#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/Device.h"
#include "audio2/Graph.h"
#include "audio2/Engine.h"
#include "audio2/UGen.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

#if defined( CINDER_COCOA )
#include "audio2/GraphAudioUnit.h"
#endif

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace audio2;

template <typename UGenT>
struct UGenNode : public Producer {
	UGenNode()	{ mTag = "UGenNode"; }
//	NoiseGen mGen;
//	SineGen mGen;
	UGenT mGen;
	virtual void render( BufferT *buffer ) override {
		mGen.render( buffer );
	}
};

struct Button {
	Button() {
		setEnabled( false );
		textColor = Color::white();
	}

	void setEnabled( bool b ) {
		if( b ) {
			backgroundColor = Color( 0.0, 0.0, 0.7 );
			title = "running";
		} else {
			backgroundColor = Color( 0.3, 0.3, 0.3 );
			title = "stopped";
		}
	}

	void draw() {
		if( ! font ) {
			font = Font( Font::getDefault().getName(), 24 );
		}
		gl::color( backgroundColor );
		gl::drawSolidRoundedRect( bounds, 5 );
		gl::drawStringCentered( title, bounds.getCenter(), textColor, font );
	}

	Rectf bounds;
	Color backgroundColor, textColor;
	string title;
	Font font;
};

class BasicTestApp : public AppNative {
  public:
	void prepareSettings( Settings *settings );
	void setup();
	void keyDown( KeyEvent event );
	void touchesBegan( TouchEvent event ) override;
	void touchesMoved( TouchEvent event ) override;
	void mouseDrag( MouseEvent event ) override;
	void update();
	void draw();

	void setupEffects();
	void setupMixer();

	GraphRef mGraph;
	shared_ptr<EffectAudioUnit> mEffect, mEffect2;

	Button mPlayButton;
};

void BasicTestApp::prepareSettings( Settings *settings )
{
	settings->enableMultiTouch();
}

void BasicTestApp::setup()
{
	mPlayButton.bounds = Rectf( 0, 0, 200, 80 );

	DeviceRef device = Device::getDefaultOutput();

	LOG_V << "device name: " << device->getName() << endl;
	console() << "\t input channels: " << device->getNumInputChannels() << endl;
	console() << "\t output channels: " << device->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << device->getSampleRate() << endl;
	console() << "\t block size: " << device->getBlockSize() << endl;

	auto output = Engine::instance()->createOutput( device );
	mGraph = Engine::instance()->createGraph();
	mGraph->setOutput( output );

//	setupEffects();
	setupMixer();

	mGraph->initialize();

	gl::enableAlphaBlending();
}

void BasicTestApp::setupEffects()
{
	auto noise = make_shared<UGenNode<NoiseGen> >();
	noise->mGen.setAmp( 0.25f );

	mEffect = make_shared<EffectAudioUnit>( kAudioUnitSubType_LowPassFilter );
	mEffect2 = make_shared<EffectAudioUnit>( kAudioUnitSubType_BandPassFilter );

	mEffect->connect( noise );
	mEffect2->connect( mEffect );
	mGraph->getOutput()->connect( mEffect2 );
}

void BasicTestApp::setupMixer()
{
	auto noise = make_shared<UGenNode<NoiseGen> >();
	noise->mGen.setAmp( 0.25f );

	auto sine = make_shared<UGenNode<SineGen> >();
	sine->mGen.setAmp( 0.25f );
	sine->mGen.setFreq( 440.0f );

	auto device = dynamic_pointer_cast<Output>( mGraph->getOutput() )->getDevice();
	sine->mGen.setSampleRate( device->getSampleRate() ); // TODO: this should be auto-configurable


	auto mixer = make_shared<MixerAudioUnit>();
	mixer->connect( noise );
	mixer->connect( sine );

	mGraph->getOutput()->connect( mixer );
}

void BasicTestApp::keyDown( KeyEvent event )
{
#if ! defined( CINDER_COCOA_TOUCH )
	if( event.getCode() == KeyEvent::KEY_SPACE ) {
		if( ! mGraph->isRunning() )
			mGraph->start();
		else
			mGraph->stop();
		mPlayButton.setEnabled( mGraph->isRunning() );
	}
#endif // ! defined( CINDER_COCOA_TOUCH )
}

void BasicTestApp::mouseDrag( MouseEvent event )
{
	if( ! mEffect )
		return;
	
	float cutoff = (getWindowHeight() - event.getY() ) * 4.0f;
//	LOG_V << "cutoff: " << cutoff << endl;

	mEffect->setParameter( kLowPassParam_CutoffFrequency, cutoff );
}

void BasicTestApp::touchesBegan( TouchEvent event )
{
//	LOG_V << "bang" << endl;
	Vec2f pos = event.getTouches().front().getPos();
	if( mPlayButton.bounds.contains( pos ) ) {
		LOG_V << "button tapped. " << endl;
		if( ! mGraph->isRunning() )
			mGraph->start();
		else
			mGraph->stop();
		mPlayButton.setEnabled( mGraph->isRunning() );
	}
}

void BasicTestApp::touchesMoved( TouchEvent event )
{
	if( ! mEffect )
		return;
	
	Vec2f pos1 = event.getTouches().front().getPos();
	float cutoff = (getWindowHeight() - pos1.y ) * 3.0f;
//	LOG_V << "cutoff: " << cutoff << endl;

	mEffect->setParameter( kLowPassParam_CutoffFrequency, cutoff );

	if( event.getTouches().size() > 1 ) {
		Vec2f pos2 = event.getTouches()[1].getPos();
//		float distortionMix = ( getWindowHeight() - pos2.y ) / getWindowHeight();
//		LOG_V << "distortionMix: " << distortionMix << endl;
//		mEffect2->setParameter( kDistortionParam_FinalMix, distortionMix );
//		mEffect2->setParameter( kDistortionParam_PolynomialMix, distortionMix );

		float centerFreq = (getWindowHeight() - pos2.y ) * 5.0f;
		mEffect2->setParameter( kBandpassParam_CenterFrequency, centerFreq );
	}
}

void BasicTestApp::update()
{
}

void BasicTestApp::draw()
{
	gl::clear();

	mPlayButton.draw();
}

CINDER_APP_NATIVE( BasicTestApp, RendererGl )
