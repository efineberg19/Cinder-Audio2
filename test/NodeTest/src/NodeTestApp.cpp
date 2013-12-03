#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "cinder/audio2/audio.h"
#include "cinder/audio2/NodeSource.h"
#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/Scope.h"
#include "cinder/audio2/CinderAssert.h"
#include "cinder/audio2/Debug.h"

#include "../../common/AudioTestGui.h"
#include "../../../samples/common/AudioPlotUtils.h"

// TODO: implement cycle detection and add test for it that catches exception

using namespace ci;
using namespace ci::app;
using namespace std;


struct InterleavedPassThruNode : public audio2::Node {
	InterleavedPassThruNode() : Node( Format() )
	{
		setAutoEnabled();
		mChannelMode = ChannelMode::SPECIFIED;
		setNumChannels( 2 );
	}

	std::string virtual getTag() override			{ return "InterleavedPassThruNode"; }

	virtual void initialize() override
	{
		mBufferInterleaved = audio2::BufferInterleaved( getContext()->getFramesPerBlock(), 2 );
	}

	void process( audio2::Buffer *buffer ) override
	{
		CI_ASSERT( buffer->getNumChannels() == 2 );

		interleaveStereoBuffer( buffer, &mBufferInterleaved );
		deinterleaveStereoBuffer( &mBufferInterleaved, buffer );
	}

private:
	audio2::BufferInterleaved mBufferInterleaved;
};

class NodeTestApp : public AppNative {
public:
	void setup();
	void draw();

	void setupGen();
	void setup2to1();
	void setup1to2();
	void setupInterleavedPassThru();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	audio2::GainRef		mGain;
	audio2::ScopeRef		mScope;
	audio2::GenRef		mGen, mNoise;

	vector<TestWidget *> mWidgets;
	Button mPlayButton, mEnableNoiseButton, mEnableSineButton;
	VSelector mTestSelector;
	HSlider mGainSlider;

	enum GainInputBus { NOISE, SINE };
};

void NodeTestApp::setup()
{
	audio2::DeviceRef device = audio2::Device::getDefaultOutput();

	LOG_V( "device name: " << device->getName() );
	console() << "\t input channels: " << device->getNumInputChannels() << endl;
	console() << "\t output channels: " << device->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << device->getSampleRate() << endl;
	console() << "\t frames per block: " << device->getFramesPerBlock() << endl;

	auto ctx = audio2::Context::master();
	mGain = ctx->makeNode( new audio2::Gain() );
//	mGain->setValue( 0.0f );

	mGain->connect( ctx->getTarget() );

	mNoise = ctx->makeNode( new audio2::GenNoise() );

	mGen = ctx->makeNode( new audio2::GenTriangle() );
	mGen->setFreq( 440.0f );

	setupGen();

	setupUI();

	ctx->printGraph();
}

void NodeTestApp::setupGen()
{
	mGain->disconnectAllInputs();

	mGen->connect( mGain );
	mGen->start();

	mEnableNoiseButton.setEnabled( false );
	mEnableSineButton.setEnabled( true );

	mGain->getParam()->rampTo( 1.0f, 1.0f );
}

void NodeTestApp::setup2to1()
{
	// connect by appending
//	mNoise->connect( mGain );
//	mGen->addConnection( mGain );

	// connect by index
	mNoise->connect( mGain, 0, GainInputBus::NOISE );
	mGen->connect( mGain, 0, GainInputBus::SINE );

	mGen->start();
	mNoise->start();

	mEnableSineButton.setEnabled( true );
	mEnableNoiseButton.setEnabled( true );
}

// note: this enables the scope as a secondary output of mGen, and as no one ever disconnects that, it harmlessly remains when the test is switched.
void NodeTestApp::setup1to2()
{
	// either of these should work, given there are only 2 NodeSource's in this test, but the latter is a tad less work.
//	mGain->disconnectAllInputs();
	mNoise->disconnect();

	mGen->connect( mGain );
	mGen->start();

	if( ! mScope )
		mScope = audio2::Context::master()->makeNode( new audio2::Scope( audio2::Scope::Format().windowSize( 2048 ) ) );
	mGen->addConnection( mScope );

	mEnableNoiseButton.setEnabled( false );
	mEnableSineButton.setEnabled( true );
}

void NodeTestApp::setupInterleavedPassThru()
{
	mGain->disconnectAllInputs();

	auto interleaved = audio2::Context::master()->makeNode( new InterleavedPassThruNode() );
	mGen->connect( interleaved )->connect( mGain );

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
	mGainSlider.set( mGain->getValue() );
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
		mGain->setValue( mGainSlider.mValueScaled );
}

void NodeTestApp::processTap( Vec2i pos )
{
	auto ctx = audio2::Context::master();

	if( mPlayButton.hitTest( pos ) )
		ctx->setEnabled( ! ctx->isEnabled() );
	if( mGen && mEnableSineButton.hitTest( pos ) )
		mGen->setEnabled( ! mGen->isEnabled() );
	if( mNoise && mEnableNoiseButton.hitTest( pos ) ) // FIXME: this check doesn't work any more because there is always an mNoise / mGen
		mNoise->setEnabled( ! mNoise->isEnabled() );

	size_t currentIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V( "selected: " << currentTest );

		if( currentTest == "sine" )
			setupGen();
		if( currentTest == "2 to 1" )
			setup2to1();
		if( currentTest == "1 to 2" )
			setup1to2();
		if( currentTest == "interleave pass-thru" )
			setupInterleavedPassThru();

		ctx->printGraph();
	}
}

void NodeTestApp::draw()
{
	gl::clear();

	if( mScope && mScope->getNumConnectedInputs() )
		drawAudioBuffer( mScope->getBuffer(), getWindowBounds(), Vec2f( 20.0f, 20.0f ), true );

	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( NodeTestApp, RendererGl )
