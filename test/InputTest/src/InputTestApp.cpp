#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/Engine.h"
#include "audio2/audio.h"
#include "audio2/UGen.h"
#include "audio2/Debug.h"

#include "Gui.h"

// TODO: test multiple formats for input
// - make sure inputs and outputs with different samplerates somehow works correctly (which was default for my win8 laptop)

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace audio2;

struct RingMod : public Effect {
	RingMod()
	: mSineGen( 440.0f, 1.0f )
	{
		mTag = "RingMod";
 		mFormat.setWantsDefaultFormatFromParent();
	}

	virtual void render( BufferT *buffer ) override {
		size_t numSamples = buffer->at( 0 ).size();
		if( mSineBuffer.size() < numSamples )
			mSineBuffer.resize( numSamples );
		mSineGen.render( &mSineBuffer );

		for ( size_t c = 0; c < buffer->size(); c++ ) {
			vector<float> &channel = buffer->at( c );
			for( size_t i = 0; i < channel.size(); i++ )
				channel[i] *= mSineBuffer[i];
		}
	}

	SineGen mSineGen;
	vector<float> mSineBuffer;
};

class InputTestApp : public AppNative {
  public:
	void setup();
	void touchesBegan( TouchEvent event ) override;
	void touchesMoved( TouchEvent event ) override;
	void mouseDown( MouseEvent event ) override;
	void mouseDrag( MouseEvent event ) override;
	void update();
	void draw();

	void logDevices( DeviceRef input, DeviceRef output );
	void setupUI();
	void toggleGraph();

	void setupPassThrough();
	void setupInProcessOut();
	void setupInTapOut();
	void setupInTapProcessOut();

	GraphRef mGraph;
	GeneratorRef mInput;
	BufferTapRef mTap;

	Button mPlayButton;
};

void InputTestApp::setup()
{
	mGraph = Engine::instance()->createGraph();

	DeviceRef inputDevice = Device::getDefaultInput();
	DeviceRef outputDevice = Device::getDefaultOutput();

	logDevices( inputDevice, outputDevice );

	mInput = Engine::instance()->createInput( inputDevice );
	auto output = Engine::instance()->createOutput( outputDevice );
	mGraph->setRoot( output );

	//setupPassThrough();
	setupInTapOut();

	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration (before init):" << endl;
	printGraph( mGraph );

	mGraph->initialize();

	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration (after init):" << endl;
	printGraph( mGraph );

	setupUI();
}

void InputTestApp::setupPassThrough()
{
	mGraph->getOutput()->connect( mInput );
}

void InputTestApp::setupInProcessOut()
{
	auto ringMod = make_shared<RingMod>();
	ringMod->connect( mInput );
	mGraph->getOutput()->connect( ringMod );
}

void InputTestApp::setupInTapOut()
{
	// TODO: make it possible for tap size to be auto-configured to input size
	// - methinks it requires all nodes to be able to keep a blocksize

	mTap = make_shared<BufferTap>();
	mTap->connect( mInput );
	mGraph->getOutput()->connect( mTap );
}

void InputTestApp::setupInTapProcessOut()
{
	//mTap = make_shared<BufferTap>( mInput->getDevice()->getBlockSize() );
	//auto ringMod = make_shared<RingMod>();
	//mTap->connect( input );
	//ringMod->connect( mTap );
	//output->connect( ringMod );
}

void InputTestApp::logDevices( DeviceRef i, DeviceRef o )
{
	LOG_V << "input device name: " << i->getName() << endl;
	console() << "\t channels: " << i->getNumInputChannels() << endl;
	console() << "\t samplerate: " << i->getSampleRate() << endl;
	console() << "\t block size: " << i->getBlockSize() << endl;

	LOG_V << "output device name: " << o->getName() << endl;
	console() << "\t channels: " << o->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << o->getSampleRate() << endl;
	console() << "\t block size: " << o->getBlockSize() << endl;

	LOG_V << "input == output: " << boolalpha << (i == o) << dec << endl;
}

void InputTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.bounds = Rectf( 0, 0, 200, 60 );

	gl::enableAlphaBlending();
}

void InputTestApp::toggleGraph()
{
	if( ! mGraph->isRunning() )
		mGraph->start();
	else
		mGraph->stop();
}

void InputTestApp::mouseDown( MouseEvent event )
{
	if( mPlayButton.hitTest( event.getPos() ) )
		toggleGraph();
}

void InputTestApp::mouseDrag( MouseEvent event )
{
//	processEvent( event.getPos() );
}

void InputTestApp::touchesBegan( TouchEvent event )
{
	if( mPlayButton.hitTest( event.getTouches().front().getPos() ) )
		toggleGraph();
}

void InputTestApp::touchesMoved( TouchEvent event )
{
//	for( const TouchEvent::Touch &touch : getActiveTouches() ) {
//		processEvent( touch.getPos() );
//	}
}

void InputTestApp::update()
{
}

void InputTestApp::draw()
{
	gl::clear();

	if( mTap && mTap->isInitialized() ) {
		const audio2::BufferT &buffer = mTap->getBuffer();

		float padding = 20.0f;
		float waveHeight = ((float)getWindowHeight() - padding * 3 ) / (float)buffer.size();

		float yOffset = padding;
		float xScale = (float)getWindowWidth() / (float)buffer[0].size();
		for( size_t ch = 0; ch < buffer.size(); ch++ ) {
			PolyLine2f waveform;
			const audio2::ChannelT &channel = buffer[ch];
			for( size_t i = 0; i < channel.size(); i++ ) {
				float x = i * xScale;
				float y = ( channel[i] * 0.5f + 0.5f ) * waveHeight + yOffset;
				waveform.push_back( Vec2f( x, y ) );
			}
			gl::color( 0, 0.9, 0 );
			gl::draw( waveform );
			yOffset += waveHeight + padding;
		}
	}

	mPlayButton.draw();
}

CINDER_APP_NATIVE( InputTestApp, RendererGl )
