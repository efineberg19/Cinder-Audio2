#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/Device.h"
#include "audio2/Graph.h"
#include "audio2/Engine.h"
#include "audio2/Dsp.h"
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
		mGen.render( &buffer->at( 0 ) );
		for( size_t i = 1; i < buffer->size(); i++ )
			memcpy( buffer->at( i ).data(), buffer->at( 0 ).data(),  buffer->at( 0 ).size() * sizeof( float ) );
	}
};

class BasicTestApp : public AppNative {
  public:
	void setup();
	void mouseDown( MouseEvent event );	
	void update();
	void draw();

	ConsumerRef mOutput;
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
	gen->mGen.setAmp( 0.2f );
	gen->mGen.setSampleRate( output->getSampleRate() );
	gen->mGen.setFreq( 220.0f );

	outputNode->connect( gen );

	// ----- Graph stuff ------
	outputNode->initialize();
	output->initialize();
	
	outputNode->start();

	mOutput = outputNode; // keep owndership
	// ------------------------

}

void BasicTestApp::mouseDown( MouseEvent event )
{
}

void BasicTestApp::update()
{
}

void BasicTestApp::draw()
{
	// clear out the window with black
	gl::clear( Color( 0, 0, 0 ) ); 
}

CINDER_APP_NATIVE( BasicTestApp, RendererGl )
