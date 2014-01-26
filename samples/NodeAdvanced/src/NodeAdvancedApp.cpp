#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "cinder/audio2/NodeInput.h"
#include "cinder/audio2/NodeEffect.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class NodeAdvancedApp : public AppNative {
  public:
	void setup();
	void mouseDown( MouseEvent event );	
	void update();
	void draw();

	audio2::GenRef	mGen;	// Gen's generate audio signals
	audio2::GainRef mGain;	// Gain modifies the volume of the signal
};

void NodeAdvancedApp::setup()
{
	auto ctx = audio2::Context::master();
	mGen = ctx->makeNode( new audio2::GenSine( audio2::Node::Format().autoEnable() ) );
	mGain = ctx->makeNode( new audio2::Gain );

	mGen->setFreq( 220 );
	mGain->getParam()->applyRamp( 0, 0.5f, 2.0f );

	mGen >> mGain >> ctx->getOutput();

	ctx->start();
}

void NodeAdvancedApp::mouseDown( MouseEvent event )
{
}

void NodeAdvancedApp::update()
{
}

void NodeAdvancedApp::draw()
{
	gl::clear();
}

CINDER_APP_NATIVE( NodeAdvancedApp, RendererGl )
