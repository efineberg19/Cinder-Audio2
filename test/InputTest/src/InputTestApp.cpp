#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/Engine.h"
#include "audio2/Debug.h"

#include "Gui.h"

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
	void touchesBegan( TouchEvent event ) override;
	void touchesMoved( TouchEvent event ) override;
	void mouseDown( MouseEvent event ) override;
	void mouseDrag( MouseEvent event ) override;
	void update();
	void draw();

	void logDevices( DeviceRef input, DeviceRef output );
	void setupUI();
	void toggleGraph();


	GraphRef mGraph;

	Button mPlayButton;
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

	setupUI();
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

void InputTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.bounds = Rectf( 0, 0, 200, 60 );

	gl::enableAlphaBlending();
}

void InputTestApp::toggleGraph()
{
	if( ! mGraph->isRunning() )
		mGraph->start();
	else
		mGraph->stop();
}

void InputTestApp::mouseDown( MouseEvent event )
{
	if( mPlayButton.hitTest( event.getPos() ) )
		toggleGraph();
}

void InputTestApp::mouseDrag( MouseEvent event )
{
//	processEvent( event.getPos() );
}

void InputTestApp::touchesBegan( TouchEvent event )
{
	if( mPlayButton.hitTest( event.getTouches().front().getPos() ) )
		toggleGraph();
}

void InputTestApp::touchesMoved( TouchEvent event )
{
//	for( const TouchEvent::Touch &touch : getActiveTouches() ) {
//		processEvent( touch.getPos() );
//	}
}

void InputTestApp::update()
{
}

void InputTestApp::draw()
{
	gl::clear();

	mPlayButton.draw();
}

CINDER_APP_NATIVE( InputTestApp, RendererGl )
