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

struct MyGen : public Producer {
	MyGen()	{ mTag = "MyGen"; }
	NoiseGen mGen;
//	SineGen mGen;
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
			title = "playing";
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

	GraphRef mGraph;
	shared_ptr<EffectAudioUnit> mEffect;


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

	DeviceRef output2 = DeviceManager::instance()->getDefaultOutput();
	LOG_V << "testing output == output2: " << (device == output2 ? "true" : "false" ) << endl;

	auto output = Engine::instance()->createOutput( device );

	auto gen = make_shared<MyGen>();
	gen->mGen.setAmp( 0.25f );
//	gen->mGen.setSampleRate( device->getSampleRate() );
//	gen->mGen.setFreq( 440.0f );


	auto effect = make_shared<EffectAudioUnit>( kAudioUnitSubType_LowPassFilter );
	effect->connect( gen );
	output->connect( effect );

	mGraph = Engine::instance()->createGraph();
	mGraph->setOutput( output );
	mGraph->initialize();

	mEffect = effect;
	gl::enableAlphaBlending();
}

void BasicTestApp::keyDown( KeyEvent event )
{
#if ! defined( CINDER_COCOA_TOUCH )
	if( event.getCode() == KeyEvent::KEY_SPACE ) {
		if( ! mGraph->isRunning() )
			mGraph->start();
		else
			mGraph->stop();
	}
#endif // ! defined( CINDER_COCOA_TOUCH )
}

void BasicTestApp::mouseDrag( MouseEvent event )
{
	float cutoff = (getWindowHeight() - event.getY() ) * 4.0f;
//	LOG_V << "cutoff: " << cutoff << endl;

	mEffect->setParameter( kLowPassParam_CutoffFrequency, cutoff );
}

void BasicTestApp::touchesBegan( TouchEvent event )
{
	LOG_V << "bang" << endl;
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
	float cutoff = (getWindowHeight() - event.getTouches().front().getY() ) * 4.0f;
//	LOG_V << "cutoff: " << cutoff << endl;

	mEffect->setParameter( kLowPassParam_CutoffFrequency, cutoff );
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
