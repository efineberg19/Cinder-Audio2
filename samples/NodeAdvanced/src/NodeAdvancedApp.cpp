#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Rand.h"

#include "cinder/audio2/NodeInput.h"
#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/Filter.h"
#include "cinder/audio2/Scope.h"
#include "cinder/audio2/Utilities.h"

#include "../../common/AudioDrawUtils.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class NodeAdvancedApp : public AppNative {
  public:
	void setup();
	void mouseMove( MouseEvent event );
	void update();
	void draw();

	audio2::GenRef				mGen;	// Gen's generate audio signals
	audio2::FilterLowPassRef	mLowpass; // A Lowpass filter will reduce high frequency content.
	audio2::GainRef				mGain;	// Gain modifies the volume of the signal
	audio2::ScopeRef			mScope;	// Scope lets you retrieve audio samples in a thread-safe manner

	vector<size_t>	mCPentatonicScale;

	float mFreqRampTime;
};

void NodeAdvancedApp::setup()
{
	auto ctx = audio2::Context::master();

	// Here we're using a GenTriangle, which generates a triangle waveform that contains many upper harmonics.
	// To reduce the sharpness, a lowpass filter is used to cut down the higher frequences.
	mGen = ctx->makeNode( new audio2::GenTriangle( audio2::Node::Format().autoEnable() ) );
	mLowpass = ctx->makeNode( new audio2::FilterLowPass );
	mGain = ctx->makeNode( new audio2::Gain );
	mScope = ctx->makeNode( new audio2::Scope );

	mLowpass->setFreq( 400 );

	// Below we tell the Gain's Param to ramp from 0 to 0.5 over 2 seconds, making it slowly fade in.!
	mGain->getParam()->applyRamp( 0, 0.5f, 2.0f );

	mGen >> mLowpass >> mGain >> ctx->getOutput();

	// Also feed the Gain to our Scope so that we can see what the waveform looks like.
	// Because the Gain is already connected to the Gain's output on bus 0, we specify the output bus as 1.
	mGain >> mScope->bus( 1, 0 );
//	mGain->addConnection( mScope ); // this would do the same thing, but automatically deduce the next available input and output busses.

	ctx->start();

	// Below is the pentatonic notes for the C major scale from C3-C5, represented in MIDI values.
	// It is many times easier to specify musical pitches in this format, which is linear rather than in hertz, which is logorithmic.
	mCPentatonicScale = { 48, 50, 52, 55, 57, 60, 62, 64, 67, 69, 72 };

	mFreqRampTime = 0.015f;
}

void NodeAdvancedApp::mouseMove( MouseEvent event )
{
	if( ! getWindowBounds().contains( event.getPos() ) )
		return;

	float yPercent = 1.0f - (float)event.getPos().y / (float)getWindowHeight();
	mLowpass->setFreq( 200 + yPercent * 800 );

	mFreqRampTime = 0.010f + event.getX() / 5000.0f;
}

void NodeAdvancedApp::update()
{
	size_t seqPeriod = 10 * randInt( 1, 4 );

	if( getElapsedFrames() % seqPeriod == 0 ) {
		size_t index = randInt( mCPentatonicScale.size() );
		size_t midiPitch = mCPentatonicScale.at( index );
		mGen->getParamFreq()->applyRamp( audio2::toFreq( midiPitch ), mFreqRampTime );
	}
}

void NodeAdvancedApp::draw()
{
	gl::clear();

	// Draw the Scope's recorded Buffer in the upper right.
	if( mScope && mScope->getNumConnectedInputs() ) {
		Rectf scopeRect( getWindowWidth() - 210, 10, getWindowWidth() - 10, 110 );
		drawAudioBuffer( mScope->getBuffer(), scopeRect, true );
	}

	// Visualize the Gen's current pitch with a circle.

	float pitchMin = mCPentatonicScale.front();
	float pitchMax = mCPentatonicScale.back();
	float currentPitch = audio2::toMidi( mGen->getFreq() ); // MIDI values do not have to be integers for us.

	float percent = ( currentPitch - pitchMin ) / ( pitchMax - pitchMin );

	float circleX = percent * getWindowWidth();

	gl::color( 0, 0.8f, 0.8f );
	gl::drawSolidCircle( Vec2f( circleX, getWindowCenter().y ), 50 );

}

CINDER_APP_NATIVE( NodeAdvancedApp, RendererGl )
