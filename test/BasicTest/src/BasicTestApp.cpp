#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/audio.h"
#include "audio2/GeneratorNode.h"
#include "audio2/EffectNode.h"
#include "audio2/CinderAssert.h"
#include "audio2/Debug.h"

#include "Gui.h"

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace ci::audio2;

struct InterleavedPassThruNode : public Node {
	InterleavedPassThruNode() : Node( Format() )
	{
		mBufferLayout = audio2::Buffer::Layout::INTERLEAVED;
		mAutoEnabled = true;
	}

	std::string virtual getTag() override			{ return "InterleavedPassThruNode"; }

	void process( audio2::Buffer *buffer ) override
	{
		CI_ASSERT( buffer->getLayout() == audio2::Buffer::Layout::INTERLEAVED );
		CI_ASSERT( buffer->getData()[0] == buffer->getData()[1] );

		// In debug mode, this should trigger an assertion failure, since it is a user error.
		//buffer->getChannel( 0 );
	}
};

class BasicTestApp : public AppNative {
public:
	void setup();
	void draw();

	void setupSine();
	void setupNoise();
	void setupSumming();
	void setupInterleavedPassThru();
	void initContext();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	ContextRef mContext;
	GainNodeRef mGain;
	GeneratorNodeRef mSine, mNoise;

	vector<TestWidget *> mWidgets;
	Button mPlayButton, mEnableNoiseButton, mEnableSineButton;
	VSelector mTestSelector;
	HSlider mGainSlider;

	enum Bus { NOISE, SINE };
};

void BasicTestApp::setup()
{
	DeviceRef device = Device::getDefaultOutput();

	LOG_V << "device name: " << device->getName() << endl;
	console() << "\t input channels: " << device->getNumInputChannels() << endl;
	console() << "\t output channels: " << device->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << device->getSampleRate() << endl;
	console() << "\t frames per block: " << device->getNumFramesPerBlock() << endl;

	mContext = Context::instance()->createContext();
	mGain = mContext->makeNode( new GainNode() );
	mGain->setGain( 0.6f );

	auto noise = mContext->makeNode( new UGenNode<NoiseGen>() );
	noise->getUGen().setAmp( 0.25f );
	mNoise = noise;

	auto sine = mContext->makeNode( new UGenNode<SineGen>() );
	sine->getUGen().setAmp( 0.25f );
	sine->getUGen().setFreq( 440.0f );
	mSine = sine;

	setupSine();
	//setupInterleavedPassThru();

	initContext();
	setupUI();
}

void BasicTestApp::setupSine()
{
	if( mGain->isConnectedToInput( mNoise ) )
		mNoise->disconnect();

	mSine->connect( mGain )->connect( mContext->getRoot() );

	// FIXME: on MSW, this starts the gen but there is no SourceVoiceXaudio started yet
	// - option: when mSine is connected, install source voice and make it auto-enabled
	mSine->start();

	mEnableNoiseButton.setEnabled( false );
	mEnableSineButton.setEnabled( true );
}

void BasicTestApp::setupNoise()
{
	if( mGain->isConnectedToInput( mSine ) )
		mSine->disconnect();

	mNoise->connect( mGain )->connect( mContext->getRoot() );

	mNoise->start();
	mEnableSineButton.setEnabled( false );
	mEnableNoiseButton.setEnabled( true );
}

void BasicTestApp::setupSumming()
{
	// connect by appending
	mNoise->connect( mGain );
	mSine->connect( mGain )->connect( mContext->getRoot() );

	// or connect by index
//	mNoise->connect( mGain, Bus::NOISE );
//	mSine->connect( mGain, Bus::SINE )->connect( mContext->getRoot() );

	mSine->start();
	mNoise->start();

	mEnableSineButton.setEnabled( true );
	mEnableNoiseButton.setEnabled( true );
}

void BasicTestApp::setupInterleavedPassThru()
{
	auto genNode = mContext->makeNode( new UGenNode<SineGen>() );
	genNode->setAutoEnabled();
	genNode->getUGen().setAmp( 0.2f );
	genNode->getUGen().setFreq( 440.0f );
	mSine = genNode;

	auto interleaved = mContext->makeNode( new InterleavedPassThruNode() );

	genNode->connect( interleaved )->connect( mContext->getRoot() );

	mEnableSineButton.setEnabled( true );
}

void BasicTestApp::initContext()
{
	mContext->initialize();

	LOG_V << "-------------------------" << endl;
	console() << "Graph initialized, configuration:" << endl;
	printGraph( mContext );
}

void BasicTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.bounds = Rectf( 0, 0, 200, 60 );
	mWidgets.push_back( &mPlayButton );

	mTestSelector.segments.push_back( "sine" );
	mTestSelector.segments.push_back( "noise" );
	mTestSelector.segments.push_back( "sine + noise" );
	mTestSelector.bounds = Rectf( getWindowWidth() * 0.67f, 0.0f, getWindowWidth(), 160.0f );
	mWidgets.push_back( &mTestSelector );

	float width = std::min( (float)getWindowWidth() - 20.0f,  440.0f );
	Rectf sliderRect( getWindowCenter().x - width / 2.0f, 200, getWindowCenter().x + width / 2.0f, 250 );
	mGainSlider.bounds = sliderRect;
	mGainSlider.title = "Gain";
	mGainSlider.max = 1.0f;
	mGainSlider.set( mGain->getGain() );
	mWidgets.push_back( &mGainSlider );

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
	if( mGainSlider.hitTest( pos ) )
		mGain->setGain( mGainSlider.valueScaled );
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
		if( currentTest == "sine + noise" )
			setupSumming();
		initContext();

		if( running )
			mContext->start();
	}
}

void BasicTestApp::draw()
{
	gl::clear();
	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( BasicTestApp, RendererGl )
