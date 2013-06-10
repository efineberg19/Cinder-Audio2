#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class FileNodeTestApp : public AppNative {
  public:
	void setup();
	void mouseDown( MouseEvent event );	
	void update();
	void draw();
};

void FileNodeTestApp::setup()
{
}

void FileNodeTestApp::mouseDown( MouseEvent event )
{
}

void FileNodeTestApp::update()
{
}

void FileNodeTestApp::draw()
{
	gl::clear();
}

CINDER_APP_NATIVE( FileNodeTestApp, RendererGl )
