#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/audio.h"
#include "audio2/GeneratorNode.h"
#include "audio2/cocoa/FileCoreAudio.h"
#include "audio2/Plot.h"
#include "audio2/Debug.h"

#include "Gui.h"

//#define SOUND_FILE "tone440.wav"
#define SOUND_FILE "tone440L220R.wav"
//#define SOUND_FILE "tone440L220R.mp3"
//#define SOUND_FILE "Blank__Kytt_-_08_-_RSPN.mp3"

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

	void setupBufferPlayer();
	void setupFilePlayer();

	void initContext();
	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	void seek( size_t xPos );

	ContextRef mContext;
	PlayerNodeRef mPlayerNode;
	SourceFileRef mSourceFile;
	WaveformPlot mWaveformPlot;
	TapNodeRef mTap;

	vector<TestWidget *> mWidgets;
	Button mEnableGraphButton, mStartPlaybackButton, mLoopButton;
};

void FileNodeTestApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1000, 500 );
}

void FileNodeTestApp::setup()
{
	mContext = Context::instance()->createContext();

	mSourceFile = SourceFileRef( new SourceFileCoreAudio( loadResource( SOUND_FILE ), 0, 44100 ) );
	LOG_V << "output samplerate: " << mSourceFile->getSampleRate() << endl;

//	setupBufferPlayer();
	setupFilePlayer();

	initContext();
	setupUI();


	mContext->start();
	mEnableGraphButton.setEnabled( true );
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

void FileNodeTestApp::setupBufferPlayer()
{
	auto audioBuffer = mSourceFile->loadBuffer();

	LOG_V << "loaded source buffer, frames: " << audioBuffer->getNumFrames() << endl;

	mWaveformPlot.load( audioBuffer, getWindowBounds() );

	mPlayerNode = make_shared<BufferPlayerNode>( audioBuffer );
	mPlayerNode->connect( mContext->getRoot() );
}

void FileNodeTestApp::setupFilePlayer()
{
	mPlayerNode = make_shared<FilePlayerNode>( mSourceFile );
	mTap = make_shared<TapNode>( 512 );

	mPlayerNode->connect( mTap )->connect( mContext->getRoot() );
}

void FileNodeTestApp::setupUI()
{
	mEnableGraphButton.isToggle = true;
	mEnableGraphButton.titleNormal = "graph off";
	mEnableGraphButton.titleEnabled = "graph on";
	mEnableGraphButton.bounds = Rectf( 0, 0, 200, 60 );
	mWidgets.push_back( &mEnableGraphButton );

	mStartPlaybackButton.isToggle = false;
	mStartPlaybackButton.titleNormal = "sample playing";
	mStartPlaybackButton.titleEnabled = "sample stopped";
	mStartPlaybackButton.bounds = mEnableGraphButton.bounds + Vec2f( mEnableGraphButton.bounds.getWidth() + 10.0f, 0.0f );
	mWidgets.push_back( &mStartPlaybackButton );

	mLoopButton.isToggle = true;
	mLoopButton.titleNormal = "loop off";
	mLoopButton.titleEnabled = "loop on";
	mLoopButton.bounds = mStartPlaybackButton.bounds + Vec2f( mEnableGraphButton.bounds.getWidth() + 10.0f, 0.0f );
	mWidgets.push_back( &mLoopButton );

	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalMouseDrag().connect( [this] ( MouseEvent &event ) { processDrag( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );
	getWindow()->getSignalTouchesMoved().connect( [this] ( TouchEvent &event ) {
		for( const TouchEvent::Touch &touch : getActiveTouches() )
			processDrag( touch.getPos() );
	} );

	gl::enableAlphaBlending();
}

void FileNodeTestApp::processDrag( Vec2i pos )
{
	seek( pos.x );
}

void FileNodeTestApp::processTap( Vec2i pos )
{
	if( mEnableGraphButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );
	else if( mStartPlaybackButton.hitTest( pos ) )
		mPlayerNode->start();
	else if( mLoopButton.hitTest( pos ) )
		mPlayerNode->setLoop( ! mPlayerNode->getLoop() );
	else
		seek( pos.x );
}

void FileNodeTestApp::seek( size_t xPos )
{
	size_t seek = mPlayerNode->getNumFrames() * xPos / getWindowWidth();
	mPlayerNode->setReadPosition( seek );
}

void FileNodeTestApp::mouseDown( MouseEvent event )
{
//	mPlayerNode->start();

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

	float readPos = (float)getWindowWidth() * mPlayerNode->getReadPosition() / mPlayerNode->getNumFrames();

	gl::color( ColorA( 0.0f, 1.0f, 0.0f, 0.7f ) );
	gl::drawSolidRoundedRect( Rectf( readPos - 2.0f, 0, readPos + 2.0f, getWindowHeight() ), 2 );

	if( mTap && mTap->isInitialized() ) {
		const audio2::Buffer &buffer = mTap->getBuffer();

		float padding = 20.0f;
		float waveHeight = ((float)getWindowHeight() - padding * 3.0f ) / (float)buffer.getNumChannels();

		float yOffset = padding;
		float xScale = (float)getWindowWidth() / (float)buffer.getNumFrames();
		for( size_t ch = 0; ch < buffer.getNumChannels(); ch++ ) {
			PolyLine2f waveform;
			const float *channel = buffer.getChannel( ch );
			for( size_t i = 0; i < buffer.getNumFrames(); i++ ) {
				float x = i * xScale;
				float y = ( channel[i] * 0.5f + 0.5f ) * waveHeight + yOffset;
				waveform.push_back( Vec2f( x, y ) );
			}
			gl::color( 0.0f, 0.9f, 0.0f );
			gl::draw( waveform );
			yOffset += waveHeight + padding;
		}
	}


	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( FileNodeTestApp, RendererGl )
