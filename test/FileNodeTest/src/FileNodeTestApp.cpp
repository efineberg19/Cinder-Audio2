#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Timeline.h"
#include "cinder/Timer.h"

#include "cinder/audio2/dsp/Converter.h"
#include "cinder/audio2/SamplePlayer.h"
#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/Scope.h"
#include "cinder/audio2/Debug.h"

#include "Resources.h"

#include "../../common/AudioTestGui.h"
#include "../../../samples/common/AudioDrawUtils.h"

// FIXME: failure on switching tests as: buffer player -> file player -> buffer player
//		- doing this and then running, FilePlayer's readFile doesn't get any samples from SourceFileCoreAudio::read()

// TODO: test the differences in sound / performance for r8brain and core audio when upsampling ogg
// TODO: move usage of Converter to base Source class, as much as possible

// TODO: add async buffer loading test via toggle

//#define INITIAL_AUDIO_RES	RES_TONE440_WAV
//#define INITIAL_AUDIO_RES	RES_TONE440L220R_WAV
//#define INITIAL_AUDIO_RES	RES_TONE440_OGG
#define INITIAL_AUDIO_RES	RES_TONE440L220R_OGG
//#define INITIAL_AUDIO_RES	RES_CASH_MP3

using namespace ci;
using namespace ci::app;
using namespace std;


class FileNodeTestApp : public AppNative {
  public:
	void prepareSettings( Settings *settings );
	void setup();
	void mouseDown( MouseEvent event );
	void keyDown( KeyEvent event );
	void fileDrop( FileDropEvent event );
	void update();
	void draw();

	void setupBufferPlayer();
	void setupFilePlayer();
	void setSourceFile( const DataSourceRef &dataSource );

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	void seek( size_t xPos );

	void testConverter();
	void testWrite();

	audio2::SamplePlayerRef		mSamplePlayer;
	audio2::SourceFileRef		mSourceFile;
	audio2::ScopeRef			mScope;
	audio2::GainRef				mGain;
	audio2::Pan2dRef			mPan;

	WaveformPlot				mWaveformPlot;
	vector<TestWidget *>		mWidgets;
	Button						mEnableGraphButton, mStartPlaybackButton, mLoopButton;
	VSelector					mTestSelector;
	HSlider						mGainSlider, mPanSlider;

	Anim<float>					mUnderrunFade, mOverrunFade;
	Rectf						mUnderrunRect, mOverrunRect;
	bool						mSamplePlayerEnabledState;
};

void FileNodeTestApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1000, 500 );
}

void FileNodeTestApp::setup()
{
	mUnderrunFade = mOverrunFade = 0;
	mSamplePlayerEnabledState = false;

	setSourceFile( loadResource( INITIAL_AUDIO_RES ) );

	auto ctx = audio2::Context::master();

	mPan = ctx->makeNode( new audio2::Pan2d() );
//	mPan->enableMonoInputMode( false );

	mGain = ctx->makeNode( new audio2::Gain() );
	mGain->setValue( 0.6f );

	mGain->connect( mPan )->connect( ctx->getTarget() );

	setupBufferPlayer();
//	setupFilePlayer();

	setupUI();

	ctx->start();
	mEnableGraphButton.setEnabled( true );

	LOG_V( "context samplerate: " << ctx->getSampleRate() );
	ctx->printGraph();
}

void FileNodeTestApp::setupBufferPlayer()
{
	auto ctx = audio2::Context::master();

	// FIXME: There is a change the FilePlayer is reading from mSourceFile on a background thread at the same time we access this here.
	// - temp workaround is making source the current sampleplayer is stopped before doing anything with mSourceFile,
	//	 but this is obviously a sore spot and one that is sure to cause frustration.
	if( mSamplePlayer )
		mSamplePlayer->stop();

	mSourceFile->setOutputFormat( ctx->getSampleRate() );
	audio2::BufferRef audioBuffer = mSourceFile->loadBuffer();

	LOG_V( "loaded source buffer, frames: " << audioBuffer->getNumFrames() );

	mWaveformPlot.load( audioBuffer, getWindowBounds() );

	mSamplePlayer = ctx->makeNode( new audio2::BufferPlayer( audioBuffer ) );
	mSamplePlayer->connect( mGain )->connect( mPan )->connect( ctx->getTarget() );
}

void FileNodeTestApp::setupFilePlayer()
{
	auto ctx = audio2::Context::master();

//	mSourceFile->setMaxFramesPerRead( 8192 );

//	mSamplePlayer = ctx->makeNode( new audio2::FilePlayer( mSourceFile ) );
	mSamplePlayer = ctx->makeNode( new audio2::FilePlayer( mSourceFile, false ) ); // synchronous file i/o

	// TODO: it is pretty surprising when you recreate mScope here without checking if there has already been one added.
	//	- user will no longer see the old mScope, but the context still owns a reference to it, so another gets added each time we call this method.
	//		- this is also because it uses 'addConnection', instead of connect with default bus numbers.
	if( ! mScope )
		mScope = ctx->makeNode( new audio2::Scope( audio2::Scope::Format().windowSize( 1024 ) ) );

	// when these connections are called, some (Gain and Pan) will already be connected, but this is okay, they should silently no-op.

	// connect scope in sequence
//	mSamplePlayer->connect( mGain )->connect( mPan )->connect( mScope )->connect( ctx->getTarget() );

	// or connect in series (it is added to the Context's 'auto pulled list')
	mSamplePlayer->connect( mGain )->connect( mPan )->connect( ctx->getTarget() );
	mPan->addConnection( mScope );

	// this call blows the current pan -> target connection, so nothing gets to the speakers
	// FIXME: what's going on here, static_assert failing in default constructor, at shut-down???
	// - check again, I think its fixed
//	mPan->connect( mScope );

}

void FileNodeTestApp::setSourceFile( const DataSourceRef &dataSource )
{
	mSourceFile = audio2::load( dataSource );

	getWindow()->setTitle( dataSource->getFilePath().filename().string() );

	LOG_V( "SourceFile info: " );
	console() << "samplerate: " << mSourceFile->getSampleRate() << endl;
	console() << "channels: " << mSourceFile->getNumChannels() << endl;
	console() << "native samplerate: " << mSourceFile->getNativeSampleRate() << endl;
	console() << "native channels: " << mSourceFile->getNativeNumChannels() << endl;
	console() << "frames: " << mSourceFile->getNumFrames() << endl;
	console() << "metadata:\n" << mSourceFile->getMetaData() << endl;
}

void FileNodeTestApp::setupUI()
{
	const float padding = 10.0f;

	mEnableGraphButton.mIsToggle = true;
	mEnableGraphButton.mTitleNormal = "graph off";
	mEnableGraphButton.mTitleEnabled = "graph on";
	mEnableGraphButton.mBounds = Rectf( padding, padding, 200, 60 );
	mWidgets.push_back( &mEnableGraphButton );

	mStartPlaybackButton.mIsToggle = false;
	mStartPlaybackButton.mTitleNormal = "sample playing";
	mStartPlaybackButton.mTitleEnabled = "sample stopped";
	mStartPlaybackButton.mBounds = mEnableGraphButton.mBounds + Vec2f( mEnableGraphButton.mBounds.getWidth() + padding, 0.0f );
	mWidgets.push_back( &mStartPlaybackButton );

	mLoopButton.mIsToggle = true;
	mLoopButton.mTitleNormal = "loop off";
	mLoopButton.mTitleEnabled = "loop on";
	mLoopButton.mBounds = mStartPlaybackButton.mBounds + Vec2f( mEnableGraphButton.mBounds.getWidth() + padding, 0.0f );
	mWidgets.push_back( &mLoopButton );

	Vec2f sliderSize( 200.0f, 30.0f );
	Rectf selectorRect( getWindowWidth() - sliderSize.x - padding, padding, getWindowWidth() - padding, sliderSize.y * 2 + padding );
	mTestSelector.mSegments.push_back( "BufferPlayer" );
	mTestSelector.mSegments.push_back( "FilePlayer" );
	mTestSelector.mBounds = selectorRect;
	mWidgets.push_back( &mTestSelector );

	Rectf sliderRect( selectorRect.x1, selectorRect.y2 + padding, selectorRect.x2, selectorRect.y2 + padding + sliderSize.y );
//	Rectf sliderRect( getWindowWidth() - 200.0f, kPadding, getWindowWidth(), 50.0f );
	mGainSlider.mBounds = sliderRect;
	mGainSlider.mTitle = "Gain";
	mGainSlider.set( mGain->getValue() );
	mWidgets.push_back( &mGainSlider );

	sliderRect += Vec2f( 0.0f, sliderRect.getHeight() + padding );
	mPanSlider.mBounds = sliderRect;
	mPanSlider.mTitle = "Pan";
	mPanSlider.set( mPan->getPos() );
	mWidgets.push_back( &mPanSlider );

	Vec2f xrunSize( 80.0f, 26.0f );
	mUnderrunRect = Rectf( padding, getWindowHeight() - xrunSize.y - padding, xrunSize.x + padding, getWindowHeight() - padding );
	mOverrunRect = mUnderrunRect + Vec2f( xrunSize.x + padding, 0.0f );

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
	if( mGainSlider.hitTest( pos ) )
		mGain->setValue( mGainSlider.mValueScaled );
	if( mPanSlider.hitTest( pos ) )
		mPan->setPos( mPanSlider.mValueScaled );
	else if( pos.y > getWindowCenter().y )
		seek( pos.x );
}

void FileNodeTestApp::processTap( Vec2i pos )
{
	if( mEnableGraphButton.hitTest( pos ) )
		audio2::Context::master()->setEnabled( ! audio2::Context::master()->isEnabled() );
	else if( mStartPlaybackButton.hitTest( pos ) )
		mSamplePlayer->start();
	else if( mLoopButton.hitTest( pos ) )
		mSamplePlayer->setLoop( ! mSamplePlayer->getLoop() );
	else if( pos.y > getWindowCenter().y )
		seek( pos.x );

	size_t currentIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V( "selected: " << currentTest );

		if( currentTest == "BufferPlayer" )
			setupBufferPlayer();
		if( currentTest == "FilePlayer" )
			setupFilePlayer();

		audio2::Context::master()->printGraph();
	}
}

void FileNodeTestApp::seek( size_t xPos )
{
	mSamplePlayer->seek( mSamplePlayer->getNumFrames() * xPos / getWindowWidth() );
}

void FileNodeTestApp::mouseDown( MouseEvent event )
{
//	mSamplePlayer->start();

//	size_t step = mBuffer.getNumFrames() / getWindowWidth();
//    size_t xLoc = event.getX() * step;
//     LOG_V( "samples starting at " << xLoc << ":" );
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
	if( event.getCode() == KeyEvent::KEY_s )
		mSamplePlayer->seekToTime( 1.0 );
}

void FileNodeTestApp::fileDrop( FileDropEvent event )
{
	const fs::path &filePath = event.getFile( 0 );
	LOG_V( "File dropped: " << filePath );

	setSourceFile( loadFile( filePath ) );

	LOG_V( "output samplerate: " << mSourceFile->getSampleRate() );

	auto bufferPlayer = dynamic_pointer_cast<audio2::BufferPlayer>( mSamplePlayer );
	if( bufferPlayer ) {
		mSourceFile->setOutputFormat( audio2::Context::master()->getSampleRate() );
		bufferPlayer->setBuffer( mSourceFile->loadBuffer() );
		mWaveformPlot.load( bufferPlayer->getBuffer(), getWindowBounds() );
	}
	else {
		auto filePlayer = dynamic_pointer_cast<audio2::FilePlayer>( mSamplePlayer );
		CI_ASSERT_MSG( filePlayer, "expected sample player to be either BufferPlayer or FilePlayer" );

		filePlayer->setSourceFile( mSourceFile );
	}

	LOG_V( "loaded and set new source buffer, channels: " << mSourceFile->getNumChannels() << ", frames: " << mSourceFile->getNumFrames() );
	audio2::Context::master()->printGraph();
}


void FileNodeTestApp::update()
{
	// light up rects if an xrun was detected
	const float xrunFadeTime = 1.3f;
	auto filePlayer = dynamic_pointer_cast<audio2::FilePlayer>( mSamplePlayer );
	if( filePlayer ) {
		if( filePlayer->getLastUnderrun() )
			timeline().apply( &mUnderrunFade, 1.0f, 0.0f, xrunFadeTime );
		if( filePlayer->getLastOverrun() )
			timeline().apply( &mOverrunFade, 1.0f, 0.0f, xrunFadeTime );
	}

	// print SamplePlayer start / stop times
	if( mSamplePlayerEnabledState != mSamplePlayer->isEnabled() ) {
		mSamplePlayerEnabledState = mSamplePlayer->isEnabled();
		string stateStr = mSamplePlayerEnabledState ? "started" : "stopped";
		LOG_V( "mSamplePlayer " << stateStr << " at " << to_string( getElapsedSeconds() ) );
	}
}

void FileNodeTestApp::draw()
{
	gl::clear();

	auto bufferPlayer = dynamic_pointer_cast<audio2::BufferPlayer>( mSamplePlayer );
	if( bufferPlayer )
		mWaveformPlot.draw();
	else if( mScope && mScope->isInitialized() )
		drawAudioBuffer( mScope->getBuffer(), getWindowBounds() );

	float readPos = (float)getWindowWidth() * mSamplePlayer->getReadPosition() / mSamplePlayer->getNumFrames();
	gl::color( ColorA( 0, 1, 0, 0.7f ) );
	gl::drawSolidRoundedRect( Rectf( readPos - 2, 0, readPos + 2, getWindowHeight() ), 2 );

	if( mUnderrunFade > 0.0001f ) {
		gl::color( ColorA( 1, 0.5f, 0, mUnderrunFade ) );
		gl::drawSolidRect( mUnderrunRect );
		gl::drawStringCentered( "underrun", mUnderrunRect.getCenter(), Color::black() );
	}
	if( mOverrunFade > 0.0001f ) {
		gl::color( ColorA( 1, 0.5f, 0, mOverrunFade ) );
		gl::drawSolidRect( mOverrunRect );
		gl::drawStringCentered( "overrun", mOverrunRect.getCenter(), Color::black() );
	}

	drawWidgets( mWidgets );
}

void FileNodeTestApp::testConverter()
{
	audio2::BufferRef audioBuffer = mSourceFile->loadBuffer();

	size_t destSampleRate = 48000;
	size_t destChannels = 1;
	size_t sourceMaxFramesPerBlock = 512;
	auto converter = audio2::dsp::Converter::create( mSourceFile->getSampleRate(), destSampleRate, mSourceFile->getNumChannels(), destChannels, sourceMaxFramesPerBlock );

	LOG_V( "FROM samplerate: " << converter->getSourceSampleRate() << ", channels: " << converter->getSourceNumChannels() << ", frames per block: " << converter->getSourceMaxFramesPerBlock() );
	LOG_V( "TO samplerate: " << converter->getDestSampleRate() << ", channels: " << converter->getDestNumChannels() << ", frames per block: " << converter->getDestMaxFramesPerBlock() );

	audio2::BufferDynamic sourceBuffer( converter->getSourceMaxFramesPerBlock(), converter->getSourceNumChannels() );
	audio2::Buffer destBuffer( converter->getDestMaxFramesPerBlock(), converter->getDestNumChannels() );

	audio2::TargetFileRef target = audio2::TargetFile::create( "resampled.wav", converter->getDestSampleRate(), converter->getDestNumChannels() );

	size_t numFramesConverted = 0;

	Timer timer( true );

	while( numFramesConverted < audioBuffer->getNumFrames() ) {
		if( audioBuffer->getNumFrames() - numFramesConverted > sourceMaxFramesPerBlock ) {
			for( size_t ch = 0; ch < audioBuffer->getNumChannels(); ch++ )
				copy( audioBuffer->getChannel( ch ) + numFramesConverted, audioBuffer->getChannel( ch ) + numFramesConverted + sourceMaxFramesPerBlock, sourceBuffer.getChannel( ch ) );
		}
		else {
			// EOF, shrink sourceBuffer to match remaining
			sourceBuffer.setNumFrames( audioBuffer->getNumFrames() - numFramesConverted );
			for( size_t ch = 0; ch < audioBuffer->getNumChannels(); ch++ )
				copy( audioBuffer->getChannel( ch ) + numFramesConverted, audioBuffer->getChannel( ch ) + audioBuffer->getNumFrames(), sourceBuffer.getChannel( ch ) );
		}

		pair<size_t, size_t> result = converter->convert( &sourceBuffer, &destBuffer );
		numFramesConverted += result.first;

		target->write( &destBuffer, 0, result.second );
	}

	LOG_V( "seconds: " << timer.getSeconds() );
}

void FileNodeTestApp::testWrite()
{
	audio2::BufferRef audioBuffer = mSourceFile->loadBuffer();

	audio2::TargetFileRef target = audio2::TargetFile::create( "out.wav", mSourceFile->getSampleRate(), mSourceFile->getNumChannels() );

	LOG_V( "writing " << audioBuffer->getNumFrames() << " frames at samplerate: " << mSourceFile->getSampleRate() << ", num channels: " << mSourceFile->getNumChannels() );
	target->write( audioBuffer.get() );
	LOG_V( "...complete." );

//	size_t writeCount = 0;
//	while( numFramesConverted < audioBuffer->getNumFrames() ) {
//		for( size_t ch = 0; ch < audioBuffer->getNumChannels(); ch++ )
//			copy( audioBuffer->getChannel( ch ) + writeCount, audioBuffer->getChannel( ch ) + writeCount + sourceFormat.getFramesPerBlock(), sourceBuffer.getChannel( ch ) );
//	}
}

CINDER_APP_NATIVE( FileNodeTestApp, RendererGl )
