#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/Device.h"
#include "audio2/Graph.h"
#include "audio2/Engine.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace audio2;

class BasicTestApp : public AppNative {
  public:
	void setup();
	void mouseDown( MouseEvent event );	
	void update();
	void draw();
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

	OutputRef outputNode = Engine::instance()->createOutputSpeakers( output );

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
