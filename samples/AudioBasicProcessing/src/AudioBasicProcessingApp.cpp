#include "cinder/app/AppNative.h"
#include "audio2/audio.h"

#include <atomic>

using namespace ci;
using namespace ci::app;

class AudioBasicProcessingApp : public AppNative {
public:
	void setup();
	void mouseDown( MouseEvent );
	void mouseDrag( MouseEvent );
	void handleMove( Vec2f pos );
	void draw();

	audio2::VoiceRef mVoice;

	// Any time you modify a variable from both the audio thread and any other thread, you should use some sort of synchronization mechanism. std::atomic is a good choice for simple data types.
	std::atomic<float> mFreq;

	//! mPhase does not need to be atomic, since it is only updated on the audio thread once processing begins.
	float mPhase;
};

void AudioBasicProcessingApp::setup()
{
	mPhase = 0.0f;

	mVoice = audio2::Voice::create( [this] ( audio2::Buffer *buffer, size_t sampleRate ) {

		float *channel0 = buffer->getChannel( 0 );

		// generate a sine wave
//		float phaseIncr = ( mFreq / (float)sampleRate ) * 2.0f * (float)M_PI;
//		for( size_t i = 0; i < buffer->getNumFrames(); i++ )	{
//			mPhase = fmodf( mPhase + phaseIncr, 2 * M_PI );
//			channel0[i] = std::sin( mPhase );
//		}

		// generate a triangle wave
		float phaseIncr = ( mFreq / (float)sampleRate );
		for( size_t i = 0; i < buffer->getNumFrames(); i++ )	{
			mPhase = fmodf( mPhase + phaseIncr, 1.0f );
			channel0[i] = std::min( mPhase, 1 - mPhase );
		}

	} );

	audio2::play( mVoice );
}

void AudioBasicProcessingApp::mouseDown( MouseEvent event )
{
	handleMove( event.getPos() );
}

void AudioBasicProcessingApp::mouseDrag( MouseEvent event )
{
	handleMove( event.getPos() );
}

void AudioBasicProcessingApp::handleMove( Vec2f pos )
{
	mFreq = pos.x;

	float volume = 1.0f - ( (float)pos.y / (float)getWindowHeight() );
	mVoice->setVolume( volume );
}

void AudioBasicProcessingApp::draw()
{
	gl::clear();
}

CINDER_APP_NATIVE( AudioBasicProcessingApp, RendererGl )