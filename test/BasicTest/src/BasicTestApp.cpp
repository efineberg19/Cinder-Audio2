#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/audio.h"
#include "audio2/GeneratorNode.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

#include "Gui.h"


// FIXME: (msw) mixer crashed on shutdown while dsp was on
// - I think it is because the buffers aren't flushed

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace audio2;

class BasicTestApp : public AppNative {
public:
	void prepareSettings( Settings *settings );
	void setup();
	void update();
	void draw();

	void setupSine();
	void setupNoise();
	void setupMixer();
	void initContext();
	void toggleGraph();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	ContextRef mContext;
	MixerNodeRef mMixer;

	vector<TestWidget *> mWidgets;
	Button mPlayButton;
	VSelector mTestSelector;
	HSlider mNoisePanSlider, mSinePanSlider, mNoiseVolumeSlider, mFreqVolumeSlider;

	enum Bus { Noise, Sine };
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

	auto output = Context::instance()->createOutput( device );
	mContext = Context::instance()->createGraph();
	mContext->setRoot( output );

	setupSine();

	initContext();
	setupUI();
}

void BasicTestApp::setupSine()
{
	auto genNode = make_shared<UGenNode<SineGen> >();
	genNode->mGen.setAmp( 0.2f );
	genNode->mGen.setFreq( 440.0f );

	mContext->getRoot()->connect( genNode );

	mNoisePanSlider.hidden = mSinePanSlider.hidden = mNoiseVolumeSlider.hidden = mFreqVolumeSlider.hidden = true;
}

void BasicTestApp::setupNoise()
{
	auto genNode = make_shared<UGenNode<NoiseGen> >();
	genNode->mGen.setAmp( 0.2f );

	mContext->getRoot()->connect( genNode );

	mNoisePanSlider.hidden = mSinePanSlider.hidden = mNoiseVolumeSlider.hidden = mFreqVolumeSlider.hidden = true;
}

void BasicTestApp::setupMixer()
{
	auto noise = make_shared<UGenNode<NoiseGen> >();
	noise->mGen.setAmp( 0.25f );

	auto sine = make_shared<UGenNode<SineGen> >();
	sine->mGen.setAmp( 0.25f );
	sine->mGen.setFreq( 440.0f );

	mMixer = Context::instance()->createMixer();

	// connect by appending
//	mMixer->connect( noise );
//	mMixer->connect( sine );

	// or connect by index
	mMixer->connect( noise, Bus::Noise );
	mMixer->connect( sine, Bus::Sine );

	mContext->getRoot()->connect( mMixer );

	mNoisePanSlider.hidden = mSinePanSlider.hidden = mNoiseVolumeSlider.hidden = mFreqVolumeSlider.hidden = false;
}

void BasicTestApp::initContext()
{
	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (before)" << endl;
	printGraph( mContext );

	mContext->initialize();

	LOG_V << "-------------------------" << endl;
	console() << "Graph configuration: (after)" << endl;
	printGraph( mContext );

	if( mMixer ) {

		// reduce default bus volumes
		// FIXME: setting params fails before Graph::initialize(), so there isn't an audio unit yet.
		//		- can I overcome this by lazy-loading the AudioUnit, just create when first asked for?
		mMixer->setBusVolume( Bus::Noise, 0.65f );
		mMixer->setBusVolume( Bus::Sine, 0.65f );

		LOG_V << "mixer stats:" << endl;
		size_t numBusses = mMixer->getNumBusses();
		console() << "\t num busses: " << numBusses << endl;
		for( size_t i = 0; i < numBusses; i++ ) {
			console() << "\t [" << i << "] enabled: " << boolalpha << mMixer->isBusEnabled( i ) << dec;
			console() << ", volume: " << mMixer->getBusVolume( i );
			console() << ", pan: " << mMixer->getBusPan( i ) << endl;
		}

		mNoisePanSlider.set( mMixer->getBusPan( Bus::Noise ) );
		mSinePanSlider.set( mMixer->getBusPan( Bus::Sine ) );
		mNoiseVolumeSlider.set( mMixer->getBusVolume( Bus::Noise ) );
		mFreqVolumeSlider.set( mMixer->getBusVolume( Bus::Sine ) );
	}
}

void BasicTestApp::toggleGraph()
{
	if( ! mContext->isRunning() )
		mContext->start();
	else
		mContext->stop();
}

void BasicTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.bounds = Rectf( 0, 0, 200, 60 );
	mWidgets.push_back( &mPlayButton );

	mTestSelector.segments.push_back( "sine" );
	mTestSelector.segments.push_back( "noise" );
	mTestSelector.segments.push_back( "mixer" );
	mTestSelector.bounds = Rectf( getWindowCenter().x + 100, 0.0f, getWindowWidth(), 160.0f );
	mWidgets.push_back( &mTestSelector );

	float width = std::min( (float)getWindowWidth() - 20.0f,  440.0f );
	Rectf sliderRect( getWindowCenter().x - width / 2.0f, 200, getWindowCenter().x + width / 2.0f, 250 );
	mNoisePanSlider.bounds = sliderRect;
	mNoisePanSlider.title = "Pan (Noise)";
	mNoisePanSlider.min = -1.0f;
	mNoisePanSlider.max = 1.0f;
	mWidgets.push_back( &mNoisePanSlider );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mSinePanSlider.bounds = sliderRect;
	mSinePanSlider.title = "Pan (Freq)";
	mSinePanSlider.min = -1.0f;
	mSinePanSlider.max = 1.0f;
	mWidgets.push_back( &mSinePanSlider );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mNoiseVolumeSlider.bounds = sliderRect;
	mNoiseVolumeSlider.title = "Volume (Noise)";
	mNoiseVolumeSlider.max = 1.0f;
	mWidgets.push_back( &mNoiseVolumeSlider );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mFreqVolumeSlider.bounds = sliderRect;
	mFreqVolumeSlider.title = "Volume (Freq)";
	mFreqVolumeSlider.max = 1.0f;
	mWidgets.push_back( &mFreqVolumeSlider );

	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalMouseDrag().connect( [this] ( MouseEvent &event ) { processDrag( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );
	getWindow()->getSignalTouchesMoved().connect( [this] ( TouchEvent &event ) {
		for( const TouchEvent::Touch &touch : getActiveTouches() )
			processDrag( touch.getPos() );
	} );

	gl::enableAlphaBlending();
}

void BasicTestApp::processDrag( Vec2i pos )
{
	if( mMixer ) {
		if( mNoisePanSlider.hitTest( pos ) )
			mMixer->setBusPan( Bus::Noise, mNoisePanSlider.valueScaled );
		if( mSinePanSlider.hitTest( pos ) )
			mMixer->setBusPan( Bus::Sine, mSinePanSlider.valueScaled );
		if( mNoiseVolumeSlider.hitTest( pos ) )
			mMixer->setBusVolume( Bus::Noise, mNoiseVolumeSlider.valueScaled );
		if( mFreqVolumeSlider.hitTest( pos ) )
			mMixer->setBusVolume( Bus::Sine, mFreqVolumeSlider.valueScaled );
	}
}

void BasicTestApp::processTap( Vec2i pos )
{
	if( mPlayButton.hitTest( pos ) )
		toggleGraph();

	size_t currentIndex = mTestSelector.currentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.currentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V << "selected: " << currentTest << endl;

		bool running = mContext->isRunning();
		mContext->uninitialize();

		if( currentTest == "sine" ) {
			setupSine();
		}
		if( currentTest == "noise" ) {
			setupNoise();
		}
		if( currentTest == "mixer" ) {
			setupMixer();
		}
		initContext();

		if( running )
			mContext->start();
	}
}

void BasicTestApp::update()
{
}

void BasicTestApp::draw()
{
	gl::clear();

	for( auto w : mWidgets )
		w->draw();
}

CINDER_APP_NATIVE( BasicTestApp, RendererGl )
