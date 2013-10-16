#include "cinder/app/AppNative.h"
#include "audio2/audio.h"

#include "Resources.h"

using namespace ci;
using namespace ci::app;

class AudioBasicProcessingApp : public AppNative {
public:
	void setup();
	void mouseDown( MouseEvent );
	void draw();

	audio2::VoiceRef mVoice;
};

void AudioBasicProcessingApp::setup()
{
	mVoice = audio2::VoiceSamplePlayer::create( loadResource( RES_DRAIN_OGG ) );


//	mVoice = audio2::Voice::create( [this] ( Buffer *buffer, Context *context ) {	/* do processing */ } );

//	mVoice = audio2::makeVoice( loadResource( RES_DRAIN_OGG ), audio2::VoiceOptions().enablePan() );
}

void AudioBasicProcessingApp::mouseDown( MouseEvent event )
{
	float volume = 1.0f - ( (float)event.getPos().y / (float)getWindowHeight() );
	float pan = (float)event.getPos().x / (float)getWindowWidth();

	mVoice->setVolume( volume );
	mVoice->setPan( pan );

	audio2::play( mVoice );
}

void AudioBasicProcessingApp::draw()
{
	gl::clear();
}

CINDER_APP_NATIVE( AudioBasicProcessingApp, RendererGl )