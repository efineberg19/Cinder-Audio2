#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/audio.h"
#include "audio2/GeneratorNode.h"
#include "audio2/Debug.h"

#include "Gui.h"

#include <Accelerate/Accelerate.h>


using namespace ci;
using namespace ci::app;
using namespace std;

using namespace audio2;

// TODO NEXT: simpl impl ala https://github.com/hoddez/FFTAccelerate/blob/master/FFTAccelerate/FFTAccelerate.cpp
// - but, I really just want the magnitude component, which is shown at:
// - http://stackoverflow.com/a/3534926/506584
// - and http://gerrybeauregard.wordpress.com/2013/01/28/using-apples-vdspaccelerate-fft/

class SpectrumTapNode : public Node {
public:
	// TODO: there should be multiple params, such as window size, fft size (so there can be padding)
	//! \note num FFT bins is half fftSize
	SpectrumTapNode( size_t fftSize = 512 ) {

		vDSP_Length log2n = log2f( fftSize );
		mFftSetup = vDSP_create_fftsetup( log2n, FFT_RADIX2 );
		CI_ASSERT( mFftSetup );
		
	}
	virtual ~SpectrumTapNode() {
		vDSP_destroy_fftsetup( mFftSetup );
	}

	virtual void initialize() override {

	}

	virtual void process( audio2::Buffer *buffer ) override {

	}

private:
//	std::vector<std::unique_ptr<RingBuffer> > mRingBuffers; // TODO: layout this out flat
	audio2::Buffer mCopiedBuffer;
	size_t mNumBufferedFrames;

	FFTSetup mFftSetup;
};


class SpectrumTapTestApp : public AppNative {
  public:
	void setup();
	void update();
	void draw();

	void initContext();
	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );
	void seek( size_t xPos );

	ContextRef mContext;
	PlayerNodeRef mPlayerNode;
	SourceFileRef mSourceFile;

	vector<TestWidget *> mWidgets;
	Button mEnableGraphButton, mStartPlaybackButton, mLoopButton;

};

void SpectrumTapTestApp::setup()
{
	mContext = Context::instance()->createContext();

	DataSourceRef dataSource = loadResource( "tone440.wav" );
	mSourceFile = SourceFile::create( dataSource, 0, 44100 );
	LOG_V << "output samplerate: " << mSourceFile->getSampleRate() << endl;

	auto audioBuffer = mSourceFile->loadBuffer();

	LOG_V << "loaded source buffer, frames: " << audioBuffer->getNumFrames() << endl;


	mPlayerNode = make_shared<BufferPlayerNode>( audioBuffer );

	// TODO: create SpectrumTapNode here

	mPlayerNode->connect( mContext->getRoot() );

	initContext();
	setupUI();


	mContext->start();
	mEnableGraphButton.setEnabled( true );

}

void SpectrumTapTestApp::initContext()
{
	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (before)" << endl;
	printGraph( mContext );

	mContext->initialize();

	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (after)" << endl;
	printGraph( mContext );
}

void SpectrumTapTestApp::setupUI()
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


void SpectrumTapTestApp::seek( size_t xPos )
{
	size_t seek = mPlayerNode->getNumFrames() * xPos / getWindowWidth();
	mPlayerNode->setReadPosition( seek );
}
void SpectrumTapTestApp::processDrag( Vec2i pos )
{
	seek( pos.x );
}

void SpectrumTapTestApp::processTap( Vec2i pos )
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

void SpectrumTapTestApp::update()
{
}

void SpectrumTapTestApp::draw()
{
	gl::clear();

	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( SpectrumTapTestApp, RendererGl )
