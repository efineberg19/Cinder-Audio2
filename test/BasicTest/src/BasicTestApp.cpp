#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/Device.h"
#include "audio2/Graph.h"
#include "audio2/Engine.h"
#include "audio2/UGen.h"
#include "audio2/audio.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

#if defined( CINDER_COCOA )
#include "audio2/GraphAudioUnit.h"
#endif

#include "Gui.h"

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace audio2;

template <typename UGenT>
struct UGenNode : public Producer {
	UGenNode()	{
		mTag = "UGenNode";
		mFormat.setWantsDefaultFormatFromParent();
	}

	virtual void initialize() override {
		mGen.setSampleRate( mFormat.getSampleRate() );
	}

	virtual void render( BufferT *buffer ) override {
		mGen.render( buffer );
	}

	UGenT mGen;
};

class BasicTestApp : public AppNative {
public:
	void prepareSettings( Settings *settings );
	void setup();
	void keyDown( KeyEvent event );
	void touchesBegan( TouchEvent event ) override;
	void touchesMoved( TouchEvent event ) override;
	void mouseDown( MouseEvent event ) override;
	void mouseDrag( MouseEvent event ) override;
	void update();
	void draw();

	void setupBasic();
	void setupMixer();
	void toggleGraph();

	void setupUI();
	void processEvent( Vec2i pos );

	GraphRef mGraph;
	MixerRef mMixer;

	Button mPlayButton;
	HSlider mNoisePanSlider, mFreqPanSlider, mLowpassCutoffSlider, mBandPassCenterSlider;
};

void BasicTestApp::prepareSettings( Settings *settings )
{
}

void BasicTestApp::setup()
{

	DeviceRef device = Device::getDefaultOutput();

	LOG_V << "device name: " << device->getName() << endl;
	console() << "\t input channels: " << device->getNumInputChannels() << endl;
	console() << "\t output channels: " << device->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << device->getSampleRate() << endl;
	console() << "\t block size: " << device->getBlockSize() << endl;

	auto output = Engine::instance()->createOutput( device );
	mGraph = Engine::instance()->createGraph();
	mGraph->setOutput( output );

	//setupBasic();
	setupMixer();

	mGraph->initialize();

	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration:" << endl;
	printGraph( mGraph );

	if( mMixer ) {
		LOG_V << "mixer stats:" << endl;
		size_t numBusses = mMixer->getNumBusses();
		console() << "\t num busses: " << numBusses << endl;
		for( size_t i = 0; i < numBusses; i++ ) {
			console() << "\t [" << i << "] enabled: " << boolalpha << mMixer->isBusEnabled( i ) << dec;
			console() << ", volume: " << mMixer->getBusVolume( i );
			console() << ", pan: " << mMixer->getBusPan( i ) << endl;
		}
	}

	setupUI();
}

void BasicTestApp::setupBasic()
{
	//auto genNode = make_shared<UGenNode<NoiseGen> >();
	//genNode->mGen.setAmp( 0.2f );

	auto genNode = make_shared<UGenNode<SineGen> >();
	genNode->mGen.setAmp( 0.2f );
	genNode->mGen.setFreq( 440.0f );

	mGraph->getOutput()->connect( genNode );
}

void BasicTestApp::setupMixer()
{
	auto noise = make_shared<UGenNode<NoiseGen> >();
	noise->mGen.setAmp( 0.25f );

	auto sine = make_shared<UGenNode<SineGen> >();
	sine->mGen.setAmp( 0.25f );
	sine->mGen.setFreq( 440.0f );

	mMixer = Engine::instance()->createMixer();
	mMixer->connect( noise );
	mMixer->connect( sine );

	mGraph->getOutput()->connect( mMixer );
}

void BasicTestApp::toggleGraph()
{
	if( ! mGraph->isRunning() )
		mGraph->start();
	else
		mGraph->stop();
}

void BasicTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.bounds = Rectf( 0, 0, 200, 60 );

	float width = std::min( (float)getWindowWidth() - 20.0f,  440.0f );
	Rectf sliderRect( getWindowCenter().x - width / 2.0f, 200, getWindowCenter().x + width / 2.0f, 250 );
	mNoisePanSlider.bounds = sliderRect;
	mNoisePanSlider.title = "Pan (Noise)";
	mNoisePanSlider.min = -1.0f;
	mNoisePanSlider.max = 1.0f;

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mFreqPanSlider.bounds = sliderRect;
	mFreqPanSlider.title = "Pan (Freq)";
	mFreqPanSlider.min = -1.0f;
	mFreqPanSlider.max = 1.0f;

	//sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	//mLowpassCutoffSlider.bounds = sliderRect;
	//mLowpassCutoffSlider.title = "Lowpass Cutoff (Noise)";
	//mLowpassCutoffSlider.max = 1500.0f;

	//sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	//mBandPassCenterSlider.bounds = sliderRect;
	//mBandPassCenterSlider.title = "Bandpass Center (Noise)";
	//mBandPassCenterSlider.min = 500.0f;
	//mBandPassCenterSlider.max = 2500.0f;

	gl::enableAlphaBlending();
}

void BasicTestApp::keyDown( KeyEvent event )
{
}

void BasicTestApp::processEvent( Vec2i pos )
{
	if( mMixer ) {
		if( mNoisePanSlider.hitTest( pos ) )
			mMixer->setBusPan( 0, mNoisePanSlider.valueScaled );
		if( mFreqPanSlider.hitTest( pos ) )
			mMixer->setBusPan( 1, mFreqPanSlider.valueScaled );
	}

	//if( mEffect ) {
	//	if( mLowpassCutoffSlider.hitTest( pos ) )
	//		mEffect->setParameter( kLowPassParam_CutoffFrequency, mLowpassCutoffSlider.valueScaled );
	//}

	//if( mEffect2 ) {
	//	if( mBandPassCenterSlider.hitTest( pos ) )
	//		mEffect2->setParameter( kBandpassParam_CenterFrequency, mBandPassCenterSlider.valueScaled );
	//}
}

void BasicTestApp::mouseDown( MouseEvent event )
{
	if( mPlayButton.hitTest( event.getPos() ) )
		toggleGraph();
}

void BasicTestApp::mouseDrag( MouseEvent event )
{
	processEvent( event.getPos() );
}

void BasicTestApp::touchesBegan( TouchEvent event )
{
	if( mPlayButton.hitTest( event.getTouches().front().getPos() ) )
		toggleGraph();
}

void BasicTestApp::touchesMoved( TouchEvent event )
{
	for( const TouchEvent::Touch &touch : getActiveTouches() ) {
		processEvent( touch.getPos() );
	}
}

void BasicTestApp::update()
{
}

void BasicTestApp::draw()
{
	gl::clear();

	mPlayButton.draw();
	mNoisePanSlider.draw();
	mFreqPanSlider.draw();
	mLowpassCutoffSlider.draw();
	mBandPassCenterSlider.draw();
}

CINDER_APP_NATIVE( BasicTestApp, RendererGl )
