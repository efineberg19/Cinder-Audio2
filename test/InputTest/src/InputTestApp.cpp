#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/Engine.h"
#include "audio2/Debug.h"

// TODO: test InputAudioUnit -> output
//	- different devices
//	- same device
// TODO: test InputAudioUnit -> tap -> output
// TODO: test InputAudioUnit -> generic process -> output

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace audio2;

class InputTestApp : public AppNative {
  public:
	void setup();
	void mouseDown( MouseEvent event );	
	void update();
	void draw();

	void logDevices( DeviceRef input, DeviceRef output );

	GraphRef mGraph;
};

void InputTestApp::setup()
{
	mGraph = Engine::instance()->createGraph();

	DeviceRef inputDevice = Device::getDefaultInput();
	DeviceRef outputDevice = Device::getDefaultOutput();

	logDevices( inputDevice, outputDevice );

	ProducerRef input = Engine::instance()->createInput( inputDevice );
	ConsumerRef output = Engine::instance()->createOutput( outputDevice );


	output->connect( input );
	mGraph->setOutput( output );
	mGraph->initialize();
}

void InputTestApp::logDevices( DeviceRef i, DeviceRef o )
{
	LOG_V << "input device name: " << i->getName() << endl;
	console() << "\t channels: " << i->getNumInputChannels() << endl;
	console() << "\t samplerate: " << i->getSampleRate() << endl;
	console() << "\t block size: " << i->getBlockSize() << endl;

	LOG_V << "output device name: " << o->getName() << endl;
	console() << "\t channels: " << o->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << o->getSampleRate() << endl;
	console() << "\t block size: " << o->getBlockSize() << endl;

	LOG_V << "input == output: " << boolalpha << (i == o) << dec << endl;
}

void InputTestApp::mouseDown( MouseEvent event )
{
	if( mGraph->isRunning() )
		mGraph->stop();
	else
		mGraph->start();
}

void InputTestApp::update()
{
}

void InputTestApp::draw()
{
	// clear out the window with black
	gl::clear( Color( 0, 0, 0 ) ); 
}

CINDER_APP_NATIVE( InputTestApp, RendererGl )
