#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/audio.h"
#include "audio2/NodeSource.h"
#include "audio2/NodeEffect.h"
#include "audio2/CinderAssert.h"
#include "audio2/Debug.h"

#include "Gui.h"

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace ci::audio2;

// TODO: consider whether its a good idea to control node auto-enabling during initialization
//   - it better matches the dynamic connections and how they can effect multiple nodes in a graph
//	 - it causes more work in the xaudio case, probably not others.
//   - start() / stop() are getting called sync from the main thread, so they cannot explicitly begin processing audio.
//			- Once again, was really only a weird xaudio issue, but I found a workaround.

struct InterleavedPassThruNode : public Node {
	InterleavedPassThruNode() : Node( Format() )
	{
		setAutoEnabled();
		mChannelMode = ChannelMode::SPECIFIED;
		setNumChannels( 2 );
	}

	std::string virtual getTag() override			{ return "InterleavedPassThruNode"; }

	virtual void initialize() override
	{
		mBufferInterleaved = BufferInterleaved( getContext()->getFramesPerBlock(), 2 );
	}

	void process( audio2::Buffer *buffer ) override
	{
		CI_ASSERT( buffer->getNumChannels() == 2 );

		interleaveStereoBuffer( buffer, &mBufferInterleaved );
		deinterleaveStereoBuffer( &mBufferInterleaved, buffer );
	}

private:
	BufferInterleaved mBufferInterleaved;
};

class NodeTestApp : public AppNative {
public:
	void setup();
	void draw();

	void setupSine();
	void setupNoiseReverse();
	void setupSumming();
	void setupInterleavedPassThru();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	Context* mContext;
	NodeGainRef mGain;
	NodeSourceRef mSine, mNoise;

	vector<TestWidget *> mWidgets;
	Button mPlayButton, mEnableNoiseButton, mEnableSineButton;
	VSelector mTestSelector;
	HSlider mGainSlider;

	enum Bus { NOISE, SINE };
};

void NodeTestApp::setup()
{
	DeviceRef device = Device::getDefaultOutput();

	LOG_V << "device name: " << device->getName() << endl;
	console() << "\t input channels: " << device->getNumInputChannels() << endl;
	console() << "\t output channels: " << device->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << device->getSampleRate() << endl;
	console() << "\t frames per block: " << device->getFramesPerBlock() << endl;

	mContext = Context::hardwareInstance();
	mGain = mContext->makeNode( new NodeGain() );
	mGain->setGain( 0.6f );

	auto noise = mContext->makeNode( new NodeGen<NoiseGen>() );
	noise->getGen().setAmp( 0.25f );
	mNoise = noise;

	auto sine = mContext->makeNode( new NodeGen<SineGen>() );
	sine->getGen().setAmp( 0.25f );
	sine->getGen().setFreq( 440.0f );
	mSine = sine;

	setupSine();

	setupUI();

	printGraph( mContext );
}

void NodeTestApp::setupSine()
{
	mGain->disconnect();

	// TODO: on msw, the 0 index for gain -> target is needed when switching between different tests,
	// but not cocoa (check). see if this can be avoided
	mSine->connect( mGain, 0 )->connect( mContext->getTarget(), 0 );

	mSine->start();

	mEnableNoiseButton.setEnabled( false );
	mEnableSineButton.setEnabled( true );
}

void NodeTestApp::setupNoiseReverse()
{
	mGain->disconnect();

	mContext->getTarget()->setInput( mGain, 0 );
	mGain->setInput( mNoise, 0 );

	mNoise->start();
	mEnableSineButton.setEnabled( false );
	mEnableNoiseButton.setEnabled( true );
}

void NodeTestApp::setupSumming()
{
	// connect by appending
	//mNoise->connect( mGain );
	//mSine->connect( mGain )->connect( mContext->getTarget(), 0 );

	// connect by index
	mGain->getInputs().resize( 2 );
	mNoise->connect( mGain, Bus::NOISE );
	mSine->connect( mGain, Bus::SINE )->connect( mContext->getTarget(), 0 );

	mSine->start();
	mNoise->start();

	mEnableSineButton.setEnabled( true );
	mEnableNoiseButton.setEnabled( true );
}

void NodeTestApp::setupInterleavedPassThru()
{
	mGain->disconnect();

	auto interleaved = mContext->makeNode( new InterleavedPassThruNode() );
	mSine->connect( interleaved )->connect( mGain, 0 )->connect( mContext->getTarget(), 0 );

	mEnableNoiseButton.setEnabled( false );
	mEnableSineButton.setEnabled( true );
}

void NodeTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.mBounds = Rectf( 0, 0, 200, 60 );
	mWidgets.push_back( &mPlayButton );

	mTestSelector.mSegments.push_back( "sine" );
	mTestSelector.mSegments.push_back( "noise (reverse)" );
	mTestSelector.mSegments.push_back( "sine + noise" );
	mTestSelector.mSegments.push_back( "interleave pass-thru" );
	mTestSelector.mBounds = Rectf( (float)getWindowWidth() * 0.67f, 0.0f, (float)getWindowWidth(), 160.0f );
	mWidgets.push_back( &mTestSelector );

	float width = std::min( (float)getWindowWidth() - 20.0f,  440.0f );
	Rectf sliderRect( getWindowCenter().x - width / 2.0f, 200, getWindowCenter().x + width / 2.0f, 250 );
	mGainSlider.mBounds = sliderRect;
	mGainSlider.mTitle = "Gain";
	mGainSlider.mMax = 1.0f;
	mGainSlider.set( mGain->getGain() );
	mWidgets.push_back( &mGainSlider );

	mEnableSineButton.mIsToggle = true;
	mEnableSineButton.mTitleNormal = "sine disabled";
	mEnableSineButton.mTitleEnabled = "sine enabled";
	mEnableSineButton.mBounds = Rectf( 0, 70, 200, 120 );
	mWidgets.push_back( &mEnableSineButton );

	mEnableNoiseButton.mIsToggle = true;
	mEnableNoiseButton.mTitleNormal = "noise disabled";
	mEnableNoiseButton.mTitleEnabled = "noise enabled";
	mEnableNoiseButton.mBounds = mEnableSineButton.mBounds + Vec2f( 0.0f, mEnableSineButton.mBounds.getHeight() + 10.0f );
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

void NodeTestApp::processDrag( Vec2i pos )
{
	if( mGainSlider.hitTest( pos ) )
		mGain->setGain( mGainSlider.mValueScaled );
}

void NodeTestApp::processTap( Vec2i pos )
{
	if( mPlayButton.hitTest( pos ) )
		mContext->setEnabled( ! mContext->isEnabled() );
	if( mSine && mEnableSineButton.hitTest( pos ) )
		mSine->setEnabled( ! mSine->isEnabled() );
	if( mNoise && mEnableNoiseButton.hitTest( pos ) ) // FIXME: this check doesn't work any more because there is always an mNoise / mSine
		mNoise->setEnabled( ! mNoise->isEnabled() );

	size_t currentIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V << "selected: " << currentTest << endl;

		if( currentTest == "sine" )
			setupSine();
		if( currentTest == "noise (reverse)" )
			setupNoiseReverse();
		if( currentTest == "sine + noise" )
			setupSumming();
		if( currentTest == "interleave pass-thru" )
			setupInterleavedPassThru();

		printGraph( mContext );
	}
}

void NodeTestApp::draw()
{
	gl::clear();
	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( NodeTestApp, RendererGl )
