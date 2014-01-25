#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "cinder/audio2/NodeInput.h"
#include "cinder/audio2/NodeEffect.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class NodeBasic : public AppNative {
public:
	void setup();
	void mouseDrag( MouseEvent event ) override;
	void draw();

	audio2::GenRef	mGen;	// Gen's generate audio signals
	audio2::GainRef mGain;	// Gain modifies the volume of the signal
};

void NodeBasic::setup()
{
	// a Context is required for making new audio Node's.
	auto ctx = audio2::Context::master();
	mGen = ctx->makeNode( new audio2::GenSine );
	mGain = ctx->makeNode( new audio2::Gain );

	mGen->setFreq( 220 );
	mGain->setValue( 0.5f );

	// connection can be done this way or via connect(). The Context's getOutput() is the speakers by default.
	mGen >> mGain >> ctx->getOutput();

	// Node's need to be enabled to process audio. By default NodeEffect's are already enabled,
	// while NodeSource's (GenSine in this case) need to be switched on.
	mGen->start();

	// Context also must be started. Starting and stopping this controls the entire DSP graph.
	ctx->start();
}

void NodeBasic::mouseDrag( MouseEvent event )
{
	mGen->setFreq( event.getPos().x );
	mGain->setValue( 1.0f - (float)event.getPos().y / (float)getWindowHeight() );
}

void NodeBasic::draw()
{
	gl::clear( Color::gray( mGain->getValue() ) );
}

CINDER_APP_NATIVE( NodeBasic, RendererGl )
