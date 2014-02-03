#include "cinder/app/AppNative.h"
#include "cinder/Timeline.h"
#include "cinder/audio2/Voice.h"
#include "cinder/audio2/Source.h"

#include "Resources.h"

using namespace ci;
using namespace ci::app;

class VoiceBasicApp : public AppNative {
public:
	void prepareSettings( Settings *settings );
	void setup();
	void mouseDown( MouseEvent event );
	void keyDown( KeyEvent event );
	void draw();

	audio2::VoiceRef mVoice;
};

void VoiceBasicApp::prepareSettings( Settings *settings )
{
	settings->enableMultiTouch( false );
}

void VoiceBasicApp::setup()
{
	mVoice = audio2::Voice::create( audio2::load( loadResource( RES_DRAIN_OGG ) ) );

	// possible (proposed) shortcut:
	//mVoice = audio2::makeVoice( loadResource( RES_DRAIN_OGG ) );
}

void VoiceBasicApp::mouseDown( MouseEvent event )
{
	float volume = 1.0f - (float)event.getPos().y / (float)getWindowHeight();
	float pan = (float)event.getPos().x / (float)getWindowWidth();

	mVoice->setVolume( volume );
	mVoice->setPan( pan );

	// By stopping the Voice first if it is already playing, we always begin playing from the beginning
	if( mVoice->isPlaying() )
		mVoice->stop();

	mVoice->play();
}

void VoiceBasicApp::keyDown( KeyEvent event )
{
	// space toggles the voice between playing and pausing
	if( event.getCode() == KeyEvent::KEY_SPACE ) {
		if( mVoice->isPlaying() )
			mVoice->pause();
		else
			mVoice->play();
	}
}

void VoiceBasicApp::draw()
{
	gl::clear();
}

CINDER_APP_NATIVE( VoiceBasicApp, RendererGl )