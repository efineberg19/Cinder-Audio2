#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class InputTestApp : public AppNative {
  public:
	void setup();
	void mouseDown( MouseEvent event );	
	void update();
	void draw();
};

void InputTestApp::setup()
{
}

void InputTestApp::mouseDown( MouseEvent event )
{
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
