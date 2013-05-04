#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/Device.h"
#include "audio2/Graph.h"
#include "audio2/Engine.h"
#include "audio2/UGen.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace audio2;

struct MyGen : public Producer {
//	NoiseGen mGen;
	SineGen mGen;
	virtual void render( BufferT *buffer ) override {
		mGen.render( buffer );
	}
};

class BasicTestApp : public AppNative {
  public:
	void setup();
	void keyDown( KeyEvent event );
	void touchesBegan( TouchEvent event ) override;
	void update();
	void draw();

	Graph mGraph;
};

void BasicTestApp::setup()
{

	DeviceRef output = Device::getDefaultOutput();

	LOG_V << "output name: " << output->getName() << endl;
	console() << "\t input channels: " << output->getNumInputChannels() << endl;
	console() << "\t output channels: " << output->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << output->getSampleRate() << endl;
	console() << "\t block size: " << output->getBlockSize() << endl;

	DeviceRef output2 = DeviceManager::instance()->getDefaultOutput();
	LOG_V << "testing output == output2: " << (output == output2 ? "true" : "false" ) << endl;

	auto outputNode = Engine::instance()->createOutput( output );

	auto gen = make_shared<MyGen>();
	gen->mGen.setAmp( 0.25f );
	gen->mGen.setSampleRate( output->getSampleRate() );
	gen->mGen.setFreq( 440.0f );

	outputNode->connect( gen );

	mGraph.setOutput( outputNode );
	mGraph.initialize();
}

void BasicTestApp::keyDown( KeyEvent event )
{
#if ! defined( CINDER_COCOA_TOUCH )
	if( event.getCode() == KeyEvent::KEY_SPACE ) {
		if( ! mGraph.isRunning() )
			mGraph.start();
		else
			mGraph.stop();
	}
#endif // ! defined( CINDER_COCOA_TOUCH )
}

void BasicTestApp::touchesBegan( TouchEvent event )
{
	LOG_V << "bang" << endl;
	if( ! mGraph.isRunning() )
		mGraph.start();
	else
		mGraph.stop();
}

void BasicTestApp::update()
{
}

void BasicTestApp::draw()
{
	gl::clear( Color::gray( mGraph.isRunning() ? 0.3 : 0.05 ) );
}

CINDER_APP_NATIVE( BasicTestApp, RendererGl )
