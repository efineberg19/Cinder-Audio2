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

	audio2::SourceRef mAudioSource;
};

void AudioPlaybackApp::setup()
{
	mAudioSource = audio2::load( loadResource( RES_DRAIN_OGG ) );
}

void AudioPlaybackApp::mouseDown( MouseEvent event )
{
	audio2::play( mAudioSource );
}

void AudioPlaybackApp::draw()
{
	gl::clear();
}

CINDER_APP_NATIVE( AudioPlaybackApp, RendererGl )
