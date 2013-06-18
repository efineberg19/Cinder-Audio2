#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Utilities.h"

#include "audio2/audio.h"
#include "audio2/GeneratorNode.h"
#include "audio2/Debug.h"

#include "Gui.h"

#include <Accelerate/Accelerate.h>

//#define SOUND_FILE "tone440.wav"
#define SOUND_FILE "tone440L220R.wav"
//#define SOUND_FILE "Blank__Kytt_-_08_-_RSPN.mp3"

// FIXME: fftSize = 1024 is broken

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace audio2;

// impl references:
// - http://stackoverflow.com/a/3534926/506584
// - http://gerrybeauregard.wordpress.com/2013/01/28/using-apples-vdspaccelerate-fft/
// - WebAudio's impl is in core/platform/audio/FFTFrame.h/cpp and audio/mac/FFTFrameMac.cpp

typedef std::shared_ptr<class SpectrumTapNode> SpectrumTapNodeRef;

class SpectrumTapNode : public Node {
public:
	// TODO: there should be multiple params, such as window size, fft size (so there can be padding)
	SpectrumTapNode( size_t fftSize = 512 )
	{
		if( fftSize & ( fftSize - 1 ) ) {
			LOG_V << "Warning: " << fftSize << " is not a power of 2, rounding up." << endl;
			size_t p = 1;
			while( p < fftSize )
				p *= 2;
			fftSize = p;
		}

		mFftSize = fftSize;
		mLog2FftSize = log2f( fftSize );
		LOG_V << "fftSize: " << fftSize << ", log2n: " << mLog2FftSize << endl;

	}
	virtual ~SpectrumTapNode() {
		vDSP_destroy_fftsetup( mFftSetup );
	}

	virtual void initialize() override {
		mFftSetup = vDSP_create_fftsetup( mLog2FftSize, FFT_RADIX2 );
		CI_ASSERT( mFftSetup );

		mReal.resize( mFftSize );
		mImag.resize( mFftSize );
		mSplitComplexFrame.realp = mReal.data();
		mSplitComplexFrame.imagp = mImag.data();

		mBuffer = audio2::Buffer( 1, mFftSize );
		mMagSpectrum.resize( mFftSize / 2 );
		LOG_V << "complete" << endl;
	}

	// if mBuffer size is smaller than buffer, only copy enough for mBuffer
	// if buffer size is smaller than mBuffer, just leave the rest as a pad
	// - so, copy the smaller of the two
	// TODO: specify pad, accumulate the required number of samples
	virtual void process( audio2::Buffer *buffer ) override {

		copyToInternalBuffer( buffer );

		vDSP_ctoz( ( DSPComplex *)mBuffer.getData(), 2, &mSplitComplexFrame, 1, mFftSize / 2 );
		vDSP_fft_zrip( mFftSetup, &mSplitComplexFrame, 1, mLog2FftSize, FFT_FORWARD );

		// TODO: window
 
		// Blow away the packed nyquist component.
		mImag[0] = 0.0f;

		lock_guard<mutex> lock( mMutex );

		// compute normalized magnitude spectrum
		// TODO: try using vDSP_zvabs for this, see if it's any faster (scaling would have to be a different step, but then so is convert to db)
		const float kMagScale = 1.0 / mFftSize;
		for( size_t i = 0; i < mMagSpectrum.size(); i++ ) {
			complex<float> c( mReal[i], mImag[i] );
			mMagSpectrum[i] = abs( c ) * kMagScale;
		}

//		int blarg = 2;
	}

	const vector<float>& getMagSpectrum() {
		lock_guard<mutex> lock( mMutex );
		return mMagSpectrum;
	}

private:

	// TODO: when stereo, should really be using a Converter to go stereo -> mono
	// - a good implementation will use equal-power scaling as if the mono signal was two stereo channels panned to center
	void copyToInternalBuffer( audio2::Buffer *buffer ) {
		mBuffer.zero();

		size_t numCopyFrames = std::min( buffer->getNumFrames(), mBuffer.getNumFrames() );
		size_t numSourceChannels = buffer->getNumChannels();
		if( numSourceChannels == 1 ) {
			memcpy( mBuffer.getData(), buffer->getData(), numCopyFrames * sizeof( float ) );
		}
		else {
			// naive average of all channels
			for( size_t ch = 0; ch < numSourceChannels; ch++ ) {
				for( size_t i = 0; i < numCopyFrames; i++ )
					mBuffer[i] += buffer->getChannel( ch )[i];
			}

			float scale = 1.0f / numSourceChannels;
			vDSP_vsmul( mBuffer.getData(), 1 , &scale, mBuffer.getData(), 1, numCopyFrames );
		}

	}

	mutex mMutex;

	//	std::vector<std::unique_ptr<RingBuffer> > mRingBuffers; // TODO: layout this out flat
	//	size_t mNumBufferedFrames;

	audio2::Buffer mBuffer;
	std::vector<float> mMagSpectrum;

	size_t mFftSize, mLog2FftSize;
	std::vector<float> mReal, mImag;

	FFTSetup mFftSetup;
	DSPSplitComplex mSplitComplexFrame;
};


class SpectrumTapTestApp : public AppNative {
  public:
	void prepareSettings( Settings *settings );
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

	SpectrumTapNodeRef mSpectrumTap;

	vector<TestWidget *> mWidgets;
	Button mEnableGraphButton, mStartPlaybackButton, mLoopButton;

};


void SpectrumTapTestApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1200, 500 );
}

void SpectrumTapTestApp::setup()
{
	mContext = Context::instance()->createContext();

	DataSourceRef dataSource = loadResource( SOUND_FILE );
	mSourceFile = SourceFile::create( dataSource, 0, 44100 );
	LOG_V << "output samplerate: " << mSourceFile->getSampleRate() << endl;

	auto audioBuffer = mSourceFile->loadBuffer();

	LOG_V << "loaded source buffer, frames: " << audioBuffer->getNumFrames() << endl;


	mPlayerNode = make_shared<BufferPlayerNode>( audioBuffer );

	mSpectrumTap = make_shared<SpectrumTapNode>( 512 );

	mPlayerNode->connect( mSpectrumTap )->connect( mContext->getRoot() );

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

// TODO: currently makes sense to enable processor + tap together - consider making these enabled together.
// - possible solution: add a silent flag that is settable by client
void SpectrumTapTestApp::processTap( Vec2i pos )
{
	if( mEnableGraphButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );
	else if( mStartPlaybackButton.hitTest( pos ) ) {
		mSpectrumTap->start();
		mPlayerNode->start();
	}
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

	// draw magnitude spectrum bins

	auto& mag = mSpectrumTap->getMagSpectrum();
	size_t numBins = mag.size();
	float margin = 40.0f;
	float padding = 2.0f;
	float binWidth = floorf( ( (float)getWindowWidth() - margin * 2.0f - padding * ( numBins - 1 ) ) / (float)numBins );
	float binYScaler = ( (float)getWindowHeight() - margin * 2.0f );

	Rectf bin( margin, getWindowHeight() - margin, margin + binWidth, getWindowHeight() - margin );
	for( size_t i = 0; i < numBins; i++ ) {
		float h = mag[i] * binYScaler;
		bin.y1 = bin.y2 - h;
		gl::color( 0.0f, 0.9f, 0.0f );
		gl::drawSolidRect( bin );

		bin += Vec2f( binWidth + padding, 0.0f );
	}

	auto min = min_element( mag.begin(), mag.end() );
	auto max = max_element( mag.begin(), mag.end() );

	string info = string( "min: " ) + toString( *min ) + string( ", max: " ) + toString( *max );
	gl::drawString( info, Vec2f( margin, getWindowHeight() - 30.0f ) );

	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( SpectrumTapTestApp, RendererGl )
