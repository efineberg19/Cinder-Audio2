#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/SamplePlayer.h"

#include "Resources.h"
#include "../../../samples/common/AudioDrawUtils.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class BufferPlayerApp : public AppNative {
public:
	void prepareSettings( Settings *settings );
	void setup();
	void fileDrop( FileDropEvent event );
	void mouseDown( MouseEvent event );
	void mouseDrag( MouseEvent event );
	void draw();

	audio2::GainRef				mGain;
	audio2::BufferPlayerRef		mBufferPlayer;

	WaveformPlot				mWaveformPlot;
};

void BufferPlayerApp::prepareSettings( Settings *settings )
{
	settings->enableMultiTouch( false );
}

void BufferPlayerApp::setup()
{
	auto ctx = audio2::Context::master();

	// create a SourceFile and set its output samplerate to match the Context.
	audio2::SourceFileRef sourceFile = audio2::load( loadResource( RES_DRAIN_OGG ) );
	sourceFile->setOutputFormat( ctx->getSampleRate() );

	// load the entire sound file into a BufferRef, and construct a BufferPlayer with this.
	audio2::BufferRef buffer = sourceFile->loadBuffer();
	mBufferPlayer = ctx->makeNode( new audio2::BufferPlayer( buffer ) );

	// add a Gain to reduce the volume
	mGain = ctx->makeNode( new audio2::Gain( 0.5f ) );

	// connect and start the Context
	mBufferPlayer >> mGain >> ctx->getOutput();
	ctx->start();

	// also load the buffer into our waveform visual util.
	mWaveformPlot.load( buffer, getWindowBounds() );
}

void BufferPlayerApp::fileDrop( FileDropEvent event )
{
	fs::path filePath = event.getFile( 0 );
	getWindow()->setTitle( filePath.filename().string() );

	audio2::SourceFileRef sourceFile = audio2::load( loadFile( filePath ) );

	// BufferPlayer can also load a buffer directly from the SourceFile.
	// This is safe to call on a background thread, which would alleviate blocking the UI loop.
	mBufferPlayer->loadBuffer( sourceFile );

	mWaveformPlot.load( mBufferPlayer->getBuffer(), getWindowBounds() );
}

void BufferPlayerApp::mouseDown( MouseEvent event )
{
	mBufferPlayer->start();
}

void BufferPlayerApp::mouseDrag( MouseEvent event )
{
	if( mBufferPlayer->isEnabled() )
		mBufferPlayer->seek( mBufferPlayer->getNumFrames() * event.getX() / getWindowWidth() );
}

void BufferPlayerApp::draw()
{
	gl::clear();
	gl::enableAlphaBlending();

	mWaveformPlot.draw();

	// draw the current play position
	float readPos = (float)getWindowWidth() * mBufferPlayer->getReadPosition() / mBufferPlayer->getNumFrames();
	gl::color( ColorA( 0, 1, 0, 0.7f ) );
	gl::drawSolidRoundedRect( Rectf( readPos - 2, 0, readPos + 2, (float)getWindowHeight() ), 2 );
}

CINDER_APP_NATIVE( BufferPlayerApp, RendererGl )