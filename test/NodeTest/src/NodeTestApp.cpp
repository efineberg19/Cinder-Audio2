#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "audio2/audio.h"
#include "audio2/NodeSource.h"
#include "audio2/NodeEffect.h"
#include "audio2/Scope.h"
#include "audio2/CinderAssert.h"
#include "audio2/Debug.h"

#include "Gui.h"
#include "Plot.h"

// TODO: implement cycle detection and add test for it that catches exception

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace ci::audio2;

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
	void setup2to1();
	void setup1to2();
	void setupInterleavedPassThru();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	Context*		mContext;
	NodeGainRef		mGain;
	ScopeRef		mScope;
	NodeSourceRef	mSine, mNoise;

	vector<TestWidget *> mWidgets;
	Button mPlayButton, mEnableNoiseButton, mEnableSineButton;
	VSelector mTestSelector;
	HSlider mGainSlider;

	enum GainInputBus { NOISE, SINE };
};

void NodeTestApp::setup()
{
	DeviceRef device = Device::getDefaultOutput();

	LOG_V << "device name: " << device->getName() << endl;
	console() << "\t input channels: " << device->getNumInputChannels() << endl;
	console() << "\t output channels: " << device->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << device->getSampleRate() << endl;
	console() << "\t frames per block: " << device->getFramesPerBlock() << endl;

	mContext = Context::master();
	mGain = mContext->makeNode( new NodeGain() );
	mGain->setGain( 0.6f );

	mGain->connect( mContext->getTarget() );

	auto noise = mContext->makeNode( new NodeGen<NoiseGen>() );
	noise->getGen().setAmp( 0.25f );
	mNoise = noise;

	auto sine = mContext->makeNode( new NodeGen<SineGen>() );
	sine->getGen().setAmp( 0.25f );
	sine->getGen().setFreq( 440.0f );
	mSine = sine;

	setupSine();

	setupUI();

	mContext->printGraph();
}

void NodeTestApp::setupSine()
{
	mGain->disconnectAllInputs();

	mSine->connect( mGain );
	mSine->start();

	mEnableNoiseButton.setEnabled( false );
	mEnableSineButton.setEnabled( true );
}

void NodeTestApp::setup2to1()
{
	// connect by appending
//	mNoise->connect( mGain );
//	mSine->addConnection( mGain );

	// connect by index
	mNoise->connect( mGain, 0, GainInputBus::NOISE );
	mSine->connect( mGain, 0, GainInputBus::SINE );

	mSine->start();
	mNoise->start();

	mEnableSineButton.setEnabled( true );
	mEnableNoiseButton.setEnabled( true );
}

// note: this enables the scope as a secondary output of mSine, and as no one ever disconnects that, it harmlessly remains when the test is switched.
void NodeTestApp::setup1to2()
{
	// either of these should work, given there are only 2 NodeSource's in this test, but the latter is a tad less work.
//	mGain->disconnectAllInputs();
	mNoise->disconnect();

	mSine->connect( mGain );
	mSine->start();

	if( ! mScope )
		mScope = mContext->makeNode( new Scope() );
	mSine->addConnection( mScope );

	mEnableNoiseButton.setEnabled( false );
	mEnableSineButton.setEnabled( true );
}

void NodeTestApp::setupInterleavedPassThru()
{
	mGain->disconnectAllInputs();

	auto interleaved = mContext->makeNode( new InterleavedPassThruNode() );
	mSine->connect( interleaved )->connect( mGain );

	mEnableNoiseButton.setEnabled( false );
	mEnableSineButton.setEnabled( true );
}

void NodeTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.mBounds = Rectf( 0, 0, 200, 60 );
	mWidgets.push_back( &mPlayButton );

	mTestSelector.mSegments.push_back( "sine" );
	mTestSelector.mSegments.push_back( "2 to 1" );
	mTestSelector.mSegments.push_back( "1 to 2" );
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
		if( currentTest == "2 to 1" )
			setup2to1();
		if( currentTest == "1 to 2" )
			setup1to2();
		if( currentTest == "interleave pass-thru" )
			setupInterleavedPassThru();

		mContext->printGraph();
	}
}

void NodeTestApp::draw()
{
	gl::clear();

	if( mScope && mScope->getNumConnectedInputs() )
		drawAudioBuffer( mScope->getBuffer(), getWindowBounds() );

	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( NodeTestApp, RendererGl )
