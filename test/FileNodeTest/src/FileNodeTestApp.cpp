#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "Resources.h"

#include "audio2/audio.h"
#include "audio2/Converter.h"
#include "audio2/NodeSource.h"
#include "audio2/NodeTap.h"
#include "Plot.h"
#include "audio2/Debug.h"

#include "Gui.h"

// FIXME: (mac) FilePlayerNode crash with heavy seeking, non-multithreaded
// - it's happening in SourceFileCoreAudio's read call - buffer ends might be overlapping

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace ci::audio2;

class FileNodeTestApp : public AppNative {
  public:
	void prepareSettings( Settings *settings );
	void setup();
	void mouseDown( MouseEvent event );
	void keyDown( KeyEvent event );
	void update();
	void draw();

	void setupBufferPlayer();
	void setupFilePlayer();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	void seek( size_t xPos );

	void testConverter();
	void testWrite();

	ContextRef mContext;
	NodeSamplePlayerRef mSamplePlayer;
	SourceFileRef mSourceFile;
	WaveformPlot mWaveformPlot;
	NodeTapRef mTap;

	vector<TestWidget *> mWidgets;
	Button mEnableGraphButton, mStartPlaybackButton, mLoopButton;
};

void FileNodeTestApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1000, 500 );
}

void FileNodeTestApp::setup()
{
	mContext = Context::create();
	
//	DataSourceRef dataSource = loadResource( RES_TONE440_WAV );
	//DataSourceRef dataSource = loadResource( RES_TONE440L220R_WAV );
	//DataSourceRef dataSource = loadResource( RES_TONE440L220R_FLOAT_WAV );
	//DataSourceRef dataSource = loadResource( RES_TONE440_MP3);
	//DataSourceRef dataSource = loadResource( RES_CASH_MP3 );

//	DataSourceRef dataSource = loadResource( RES_TONE440_OGG );
	DataSourceRef dataSource = loadResource( "Stevie Wonder  For Once In My Life.ogg" );

	mSourceFile = SourceFile::create( dataSource, 0, mContext->getSampleRate() );
	LOG_V << "output samplerate: " << mSourceFile->getSampleRate() << endl;

	setupBufferPlayer();
	//setupFilePlayer();

	setupUI();

	mContext->start();
	mEnableGraphButton.setEnabled( true );

	printGraph( mContext );
}

void FileNodeTestApp::setupBufferPlayer()
{
	BufferRef audioBuffer = mSourceFile->loadBuffer();

	LOG_V << "loaded source buffer, frames: " << audioBuffer->getNumFrames() << endl;

	mWaveformPlot.load( audioBuffer, getWindowBounds() );

	mSamplePlayer = mContext->makeNode( new NodeBufferPlayer( audioBuffer ) );
	mSamplePlayer->connect( mContext->getTarget() );
}

void FileNodeTestApp::setupFilePlayer()
{
	// TODO: read count should currently always be a multiple of the current block size.
	// - make sure this is enforced or make it unnecessary
//	mSourceFile->setNumFramesPerRead( 4096 );
	mSourceFile->setNumFramesPerRead( 8192 );

//	mSamplePlayer = mContext->makeNode( new NodeFilePlayer( mSourceFile ) );
	mSamplePlayer = mContext->makeNode( new NodeFilePlayer( mSourceFile, false ) ); // synchronous file i/o

	mTap = mContext->makeNode( new NodeTap( NodeTap::Format().windowSize( 512 ) ) ); // TODO: why is this hard-coded?

	mSamplePlayer->connect( mTap )->connect( mContext->getTarget() );
}

void FileNodeTestApp::setupUI()
{
	mEnableGraphButton.mIsToggle = true;
	mEnableGraphButton.mTitleNormal = "graph off";
	mEnableGraphButton.mTitleEnabled = "graph on";
	mEnableGraphButton.mBounds = Rectf( 0, 0, 200, 60 );
	mWidgets.push_back( &mEnableGraphButton );

	mStartPlaybackButton.mIsToggle = false;
	mStartPlaybackButton.mTitleNormal = "sample playing";
	mStartPlaybackButton.mTitleEnabled = "sample stopped";
	mStartPlaybackButton.mBounds = mEnableGraphButton.mBounds + Vec2f( mEnableGraphButton.mBounds.getWidth() + 10.0f, 0.0f );
	mWidgets.push_back( &mStartPlaybackButton );

	mLoopButton.mIsToggle = true;
	mLoopButton.mTitleNormal = "loop off";
	mLoopButton.mTitleEnabled = "loop on";
	mLoopButton.mBounds = mStartPlaybackButton.mBounds + Vec2f( mEnableGraphButton.mBounds.getWidth() + 10.0f, 0.0f );
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
		mSamplePlayer->start();
	else if( mLoopButton.hitTest( pos ) )
		mSamplePlayer->setLoop( ! mSamplePlayer->getLoop() );
	else
		seek( pos.x );
}

void FileNodeTestApp::seek( size_t xPos )
{
	size_t seek = mSamplePlayer->getNumFrames() * xPos / getWindowWidth();
	mSamplePlayer->setReadPosition( seek );
}

void FileNodeTestApp::mouseDown( MouseEvent event )
{
//	mSamplePlayer->start();

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

void FileNodeTestApp::keyDown( KeyEvent event )
{
	if( event.getCode() == KeyEvent::KEY_c )
		testConverter();
	if( event.getCode() == KeyEvent::KEY_w )
		testWrite();
}


void FileNodeTestApp::update()
{
}

void FileNodeTestApp::draw()
{
	gl::clear();
	mWaveformPlot.draw();

	float readPos = (float)getWindowWidth() * mSamplePlayer->getReadPosition() / mSamplePlayer->getNumFrames();

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

void FileNodeTestApp::testConverter()
{
	BufferRef audioBuffer = mSourceFile->loadBuffer();

	auto sourceFormat = Converter::Format().sampleRate( mSourceFile->getSampleRate() ).channels( mSourceFile->getNumChannels() ).framesPerBlock( 512 );
	auto destFormat = Converter::Format().sampleRate( 48000 ).channels( mSourceFile->getNumChannels() );

	size_t destFramesPerBlock = ceil( (float)sourceFormat.getFramesPerBlock() * (float)destFormat.getSampleRate() / (float)sourceFormat.getSampleRate() );

	LOG_V << "converting from:" << endl;
	console() << "\tsamplerate: " << sourceFormat.getSampleRate() << ", channels: " << sourceFormat.getChannels() << ", frames per block: " << sourceFormat.getFramesPerBlock() << endl;
	LOG_V << "to:" << endl;
	console() << "\tsamplerate: " << destFormat.getSampleRate() << ", channels: " << destFormat.getChannels() << ", frames per block: " << destFramesPerBlock << endl;

	audio2::Buffer sourceBuffer( sourceFormat.getFramesPerBlock(), sourceFormat.getChannels() );
	audio2::Buffer destBuffer( destFramesPerBlock, sourceFormat.getChannels() );

	TargetFileRef target = TargetFile::create( "resampled.wav", destFormat.getSampleRate(), destFormat.getChannels() );

	auto converter = Converter::create( sourceFormat, destFormat );
	size_t numFramesConverted = 0;

	while( numFramesConverted < audioBuffer->getNumFrames() ) {
		for( size_t ch = 0; ch < audioBuffer->getNumChannels(); ch++ )
			copy( audioBuffer->getChannel( ch ) + numFramesConverted, audioBuffer->getChannel( ch ) + numFramesConverted + sourceFormat.getFramesPerBlock(), sourceBuffer.getChannel( ch ) );

		pair<size_t, size_t> result = converter->convert( &sourceBuffer, &destBuffer );
		numFramesConverted += result.first;

		target->write( &destBuffer, 0, result.second );
	}
}

void FileNodeTestApp::testWrite()
{
	BufferRef audioBuffer = mSourceFile->loadBuffer();

	TargetFileRef target = TargetFile::create( "out.wav", mSourceFile->getSampleRate(), mSourceFile->getNumChannels() );

	LOG_V << "writing " << audioBuffer->getNumFrames() << " frames at samplerate: " << mSourceFile->getSampleRate() << ", num channels: " << mSourceFile->getNumChannels() << endl;
	target->write( audioBuffer.get() );
	LOG_V << "...complete." << endl;

//	size_t writeCount = 0;
//	while( numFramesConverted < audioBuffer->getNumFrames() ) {
//		for( size_t ch = 0; ch < audioBuffer->getNumChannels(); ch++ )
//			copy( audioBuffer->getChannel( ch ) + writeCount, audioBuffer->getChannel( ch ) + writeCount + sourceFormat.getFramesPerBlock(), sourceBuffer.getChannel( ch ) );
//	}
}

CINDER_APP_NATIVE( FileNodeTestApp, RendererGl )
