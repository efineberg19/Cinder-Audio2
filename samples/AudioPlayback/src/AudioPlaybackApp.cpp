#include "cinder/app/AppNative.h"
#include "audio2/audio.h"

#include "Resources.h"

using namespace ci;
using namespace ci::app;

class AudioPlaybackApp : public AppNative {
public:
	void setup();
	void mouseDown( MouseEvent );
	void draw();

	audio2::VoiceRef mVoice;
};

void AudioPlaybackApp::setup()
{
	mVoice = audio2::makeVoice( loadResource( RES_DRAIN_OGG ), audio2::VoiceOptions().enablePan() );
}

void AudioPlaybackApp::mouseDown( MouseEvent event )
{
	float volume = 1.0f - ( (float)event.getPos().y / (float)getWindowHeight() );
	float pan = (float)event.getPos().x / (float)getWindowWidth();

	mVoice->setVolume( volume );
	mVoice->setPan( pan );

	audio2::play( mVoice );
}

void AudioPlaybackApp::draw()
{
	gl::clear();
}

CINDER_APP_NATIVE( AudioPlaybackApp, RendererGl )