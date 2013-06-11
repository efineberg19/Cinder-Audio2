#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/audio.h"
#include "audio2/GeneratorNode.h"
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

	void initContext();
	void toggleGraph();

	ContextRef mContext;
	BufferInputNodeRef mBufferInputNode;
	WaveformPlot mWaveformPlot;
};

void FileNodeTestApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1000, 500 );
}

void FileNodeTestApp::setup()
{
	mContext = Context::instance()->createContext();
	OutputNodeRef output = Context::instance()->createOutput();
	mContext->setRoot( output );

	auto sourceFile = SourceFileCoreAudio( loadResource( "tone440.wav" ) );
	LOG_V << "output samplerate: " << sourceFile.getOutputSampleRate() << endl;

	auto audioBuffer = sourceFile.loadBuffer();

	LOG_V << "loaded source buffer, frames: " << audioBuffer->getNumFrames() << endl;

	mWaveformPlot.load( audioBuffer, getWindowBounds() );

	return;
	
	mBufferInputNode = make_shared<BufferInputNode>( audioBuffer );
	mBufferInputNode->connect( output );

	initContext();
}

void FileNodeTestApp::initContext()
{
	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (before)" << endl;
	printGraph( mContext );

	mContext->initialize();

	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (after)" << endl;
	printGraph( mContext );
}

void FileNodeTestApp::toggleGraph()
{
	if( ! mContext->isRunning() )
		mContext->start();
	else
		mContext->stop();
}

void FileNodeTestApp::mouseDown( MouseEvent event )
{
//	mBufferInputNode->start();

//	size_t step = mBuffer.getNumFrames() / getWindowWidth();
//    size_t xLoc = event.getX() * step;
//    LOG_V << "samples starting at " << xLoc << ":\n";
//    for( int i = 0; i < 100; i++ ) {
//        if( mNumChannels == 1 ) {
//            console() << mBuffer.getChannel( 0 )[xLoc + i] << ", ";
//        } else {
//            console() << "[" << mBuffer.getChannel( 0 )[xLoc + i] << ", " << mBuffer.getChannel( 0 )[xLoc + i] << "], ";
//        }
//    }
//    console() << endl;
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
