#include "cinder/app/AppNative.h"
#include "cinder/Timeline.h"
#include "cinder/audio2/Voice.h"
#include "cinder/audio2/Source.h"

#include "Resources.h"

using namespace ci;
using namespace ci::app;

class AudioBasicPlaybackApp : public AppNative {
public:
	void setup();
	void mouseDown( MouseEvent );
	void draw();

	audio2::VoiceRef mVoice;
};

void AudioBasicPlaybackApp::setup()
{
	mVoice = audio2::Voice::create( audio2::load( loadResource( RES_DRAIN_OGG ) ) );

	// possible (proposed) shortcut:
//	mVoice = audio2::makeVoice( loadResource( RES_DRAIN_OGG ) );
}

void AudioBasicPlaybackApp::mouseDown( MouseEvent event )
{
	float volume = 1.0f - (float)event.getPos().y / (float)getWindowHeight();
	float pan = (float)event.getPos().x / (float)getWindowWidth();

	mVoice->setVolume( volume );
	mVoice->setPan( pan );

	mVoice->play();
}

void AudioBasicPlaybackApp::draw()
{
	gl::clear();
}

CINDER_APP_NATIVE( AudioBasicPlaybackApp, RendererGl )