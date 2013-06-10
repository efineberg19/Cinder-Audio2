#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/cocoa/FileCoreAudio.h"
#include "audio2/Plot.h"
#include "audio2/Debug.h"

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace audio2;
using namespace audio2::cocoa;

class FileNodeTestApp : public AppNative {
  public:
	void prepareSettings( Settings *settings );
	void setup();
	void mouseDown( MouseEvent event );	
	void update();
	void draw();

	SourceBufferRef mSourceBuffer;
	WaveformPlot mWaveformPlot;
};

void FileNodeTestApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1000, 500 );
}

void FileNodeTestApp::setup()
{
	auto dataSource = loadResource( "tone440.wav" );
	auto sourceFile = SourceFileCoreAudio( dataSource );

	LOG_V << "output samplerate: " << sourceFile.getOutputSampleRate() << endl;

	mSourceBuffer = make_shared<SourceBuffer>();
	sourceFile.load( mSourceBuffer );

	LOG_V << "loaded source buffer, frames: " << mSourceBuffer->getNumFrames() << endl;

	mWaveformPlot.load( *mSourceBuffer.get(), getWindowBounds() );
}

void FileNodeTestApp::mouseDown( MouseEvent event )
{
}

void FileNodeTestApp::update()
{
}

void FileNodeTestApp::draw()
{
	gl::clear();
	gl::draw( mWaveformPlot );
}

CINDER_APP_NATIVE( FileNodeTestApp, RendererGl )
