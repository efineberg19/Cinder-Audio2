#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/audio.h"
#include "audio2/GeneratorNode.h"
#include "audio2/assert.h"
#include "audio2/Debug.h"

#include "Gui.h"

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace audio2;

struct InterleavedPassThruNode : public Node {
	InterleavedPassThruNode() : Node( Format() )
	{
		mTag = "InterleavedPassThruNode";
		mBufferLayout = audio2::Buffer::Layout::Interleaved;
		mAutoEnabled = true;
	}

	void process( audio2::Buffer *buffer ) override
	{
		CI_ASSERT( buffer->getLayout() == audio2::Buffer::Layout::Interleaved );
		CI_ASSERT( buffer->getData()[0] == buffer->getData()[1] );

		// In debug mode, this will trigger an assertion failure, since it is a user error.
		//buffer->getChannel( 0 );
	}

};

class BasicTestApp : public AppNative {
public:
	void prepareSettings( Settings *settings );
	void setup();
	void update();
	void draw();

	void setupSine();
	void setupNoise();
	void setupMixer();
	void setupInterleavedPassThru();
	void initContext();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	ContextRef mContext;
	MixerNodeRef mMixer;
	GeneratorNodeRef mSine, mNoise;

	vector<TestWidget *> mWidgets;
	Button mPlayButton, mEnableNoiseButton, mEnableSineButton;
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
	console() << "\t frames per block: " << device->getNumFramesPerBlock() << endl;

	auto output = Context::instance()->createOutput( device );
	mContext = Context::instance()->createContext();
	mContext->setRoot( output );

	setupSine();
	//setupInterleavedPassThru();

	initContext();
	setupUI();
}

void BasicTestApp::setupSine()
{
	auto genNode = make_shared<UGenNode<SineGen> >( Node::Format().channels( 1 ) );
	genNode->setAutoEnabled();
	genNode->getUGen().setAmp( 0.2f );
	genNode->getUGen().setFreq( 440.0f );

	genNode->connect( mContext->getRoot() );
	mSine = genNode;

	mEnableSineButton.setEnabled( true );
	mEnableSineButton.hidden = false;
	mNoisePanSlider.hidden = mSinePanSlider.hidden = mNoiseVolumeSlider.hidden = mFreqVolumeSlider.hidden = mEnableNoiseButton.hidden = true;
}

void BasicTestApp::setupNoise()
{
	auto genNode = make_shared<UGenNode<NoiseGen> >();
	genNode->setAutoEnabled();
	genNode->getUGen().setAmp( 0.2f );

	genNode->connect( mContext->getRoot() );
	mNoise = genNode;

	mEnableNoiseButton.setEnabled( true );
	mEnableNoiseButton.hidden = false;
	mNoisePanSlider.hidden = mSinePanSlider.hidden = mNoiseVolumeSlider.hidden = mFreqVolumeSlider.hidden = mEnableSineButton.hidden = true;
}

void BasicTestApp::setupMixer()
{
	auto noise = make_shared<UGenNode<NoiseGen> >();
	noise->setAutoEnabled();
	noise->getUGen().setAmp( 0.25f );
	mNoise = noise;

	auto sine = make_shared<UGenNode<SineGen> >();
	sine->setAutoEnabled();
	sine->getUGen().setAmp( 0.25f );
	sine->getUGen().setFreq( 440.0f );
	mSine = sine;
	
	mMixer = Context::instance()->createMixer();

	// connect by appending
//	noise->connect( mMixer );
//	sine->connect( mMixer )->connect( mContext->getRoot() );

	// or connect by index
	noise->connect( mMixer, Bus::Noise );
	sine->connect( mMixer, Bus::Sine )->connect( mContext->getRoot() );

	mEnableSineButton.setEnabled( true );
	mEnableNoiseButton.setEnabled( true );
	mNoisePanSlider.hidden = mSinePanSlider.hidden = mNoiseVolumeSlider.hidden = mFreqVolumeSlider.hidden = mEnableSineButton.hidden = mEnableNoiseButton.hidden = false;
}

// TODO: this belongs in it's own test app - one for weird conversions
void BasicTestApp::setupInterleavedPassThru()
{
	auto genNode = make_shared<UGenNode<SineGen> >();
	genNode->setAutoEnabled();
	genNode->getUGen().setAmp( 0.2f );
	genNode->getUGen().setFreq( 440.0f );
	mSine = genNode;

	auto interleaved = make_shared<InterleavedPassThruNode>();

	genNode->connect( interleaved )->connect( mContext->getRoot() );

	mEnableSineButton.setEnabled( true );
	mEnableSineButton.hidden = false;
	mNoisePanSlider.hidden = mSinePanSlider.hidden = mNoiseVolumeSlider.hidden = mFreqVolumeSlider.hidden = mEnableNoiseButton.hidden = true;
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

void BasicTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.bounds = Rectf( 0, 0, 200, 60 );
	mWidgets.push_back( &mPlayButton );

	mTestSelector.segments.push_back( "sine" );
	mTestSelector.segments.push_back( "noise" );
	mTestSelector.segments.push_back( "mixer" );
	mTestSelector.bounds = Rectf( getWindowWidth() * 0.67f, 0.0f, getWindowWidth(), 160.0f );
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


	mEnableSineButton.isToggle = true;
	mEnableSineButton.titleNormal = "sine disabled";
	mEnableSineButton.titleEnabled = "sine enabled";
	mEnableSineButton.bounds = Rectf( 0, 70, 200, 120 );
	mWidgets.push_back( &mEnableSineButton );

	mEnableNoiseButton.isToggle = true;
	mEnableNoiseButton.titleNormal = "noise disabled";
	mEnableNoiseButton.titleEnabled = "noise enabled";
	mEnableNoiseButton.bounds = mEnableSineButton.bounds + Vec2f( 0.0f, mEnableSineButton.bounds.getHeight() + 10.0f );
	mWidgets.push_back( &mEnableNoiseButton );


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
		mContext->setEnabled( ! mContext->isEnabled() );
	if( mSine && mEnableSineButton.hitTest( pos ) )
		mSine->setEnabled( ! mSine->isEnabled() );
	if( mNoise && mEnableNoiseButton.hitTest( pos ) )
		mNoise->setEnabled( ! mNoise->isEnabled() );

	size_t currentIndex = mTestSelector.currentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.currentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V << "selected: " << currentTest << endl;

		bool running = mContext->isEnabled();
		mContext->uninitialize();

		if( currentTest == "sine" )
			setupSine();
		if( currentTest == "noise" )
			setupNoise();
		if( currentTest == "mixer" )
			setupMixer();
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
	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( BasicTestApp, RendererGl )
