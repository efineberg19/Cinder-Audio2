#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "cinder/audio2/Gen.h"
#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/Scope.h"
#include "cinder/audio2/CinderAssert.h"
#include "cinder/audio2/dsp/Converter.h"
#include "cinder/audio2/Debug.h"

#include "../../common/AudioTestGui.h"
#include "../../../samples/common/AudioDrawUtils.h"

#include "cinder/audio2/Utilities.h"

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

	virtual void initialize() override
	{
		mBufferInterleaved = audio2::BufferInterleaved( getContext()->getFramesPerBlock(), 2 );
	}

	void process( audio2::Buffer *buffer ) override
	{
		CI_ASSERT( buffer->getNumChannels() == 2 );

		audio2::dsp::interleaveStereoBuffer( buffer, &mBufferInterleaved );
		audio2::dsp::deinterleaveStereoBuffer( &mBufferInterleaved, buffer );
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
	void setupAutoPulled();
	void setupFunnelCase();

	void printDefaultOutput();
	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	audio2::GainRef		mGain;
	audio2::ScopeRef	mScope;
	audio2::GenRef		mGen, mNoise;

	vector<TestWidget *>	mWidgets;
	Button					mPlayButton, mEnableNoiseButton, mEnableSineButton;
	VSelector				mTestSelector;
	HSlider					mGainSlider;

	enum InputBus { SINE, NOISE };
};

void NodeTestApp::setup()
{
	printDefaultOutput();
	
	auto ctx = audio2::master();
	mGain = ctx->makeNode( new audio2::Gain( 0.04f ) );
	mGen = ctx->makeNode( new audio2::GenSine( 440 ) );
	mNoise = ctx->makeNode( new audio2::GenNoise() );

	mScope = audio2::master()->makeNode( new audio2::Scope( audio2::Scope::Format().windowSize( 2048 ) ) );

	setupGen();
	ctx->printGraph();
	
	setupUI();
}

void NodeTestApp::setupGen()
{
	mGain->disconnectAllInputs();

	mGen >> mGain >> audio2::master()->getOutput();

	mGen->start();

	mEnableNoiseButton.setEnabled( false );
	mEnableSineButton.setEnabled( true );
}

void NodeTestApp::setup2to1()
{
	// connect by appending
//	mNoise->connect( mGain );
//	mGen->addConnection( mGain );

	// connect by index
//	mGen->connect( mGain, 0, InputBus::SINE );
//	mNoise->connect( mGain, 0, InputBus::NOISE );

	// connect by bus using operator>>
	mGen >> mGain->bus( InputBus::SINE );
	mNoise >> mGain->bus( InputBus::NOISE );

	// ???: possible?
//	mNoise->bus( 0 ) >> mGain->bus( InputBus::NOISE );

	mGain >> audio2::master()->getOutput();

	mGen->start();
	mNoise->start();

	mEnableSineButton.setEnabled( true );
	mEnableNoiseButton.setEnabled( true );
}

// note: this enables the scope as a secondary output of mGen, and as no one ever disconnects that, it harmlessly remains when the test is switched.
void NodeTestApp::setup1to2()
{
	// either of these should work, given there are only 2 NodeInput's in this test, but the latter is a tad less work.
//	mGain->disconnectAllInputs();
	mNoise->disconnect();

	// TODO: this wants to connect at bus 0, but what if mGen is already connected at a different bus?
	// - output bus should be updated to reflect the newly specified index
	mGen >> mGain >> audio2::master()->getOutput();
	mGen->start();

	mGen->addConnection( mScope );

	mEnableNoiseButton.setEnabled( false );
	mEnableSineButton.setEnabled( true );
}

void NodeTestApp::setupInterleavedPassThru()
{
	auto ctx = audio2::master();

	mGain->disconnectAllInputs();

	auto interleaved = ctx->makeNode( new InterleavedPassThruNode() );
	mGen >> interleaved >> mGain >> ctx->getOutput();
	mGen->start();

	mEnableNoiseButton.setEnabled( false );
	mEnableSineButton.setEnabled( true );
}

void NodeTestApp::setupAutoPulled()
{
	auto ctx = audio2::master();
	ctx->disconnectAllNodes();

	mGen >> mScope;

	mGen->start();

	// TODO: dsp would have been stopped by ctx->disconnectAllNodes() - why is this necessary?
	// - don't stop context if not necessary
	if( mPlayButton.mEnabled )
		ctx->start();
}

void NodeTestApp::setupFunnelCase()
{
	auto ctx = audio2::master();
	ctx->disconnectAllNodes();

	auto gain1 = ctx->makeNode( new audio2::Gain );
	auto gain2 = ctx->makeNode( new audio2::Gain );
//	auto gain2 = ctx->makeNode( new audio2::Gain( audio2::Node::Format().autoEnable( false ) ) );

	mGen >> gain1 >> mScope->bus( InputBus::SINE );
	mNoise >> gain2 >> mScope->bus( InputBus::NOISE );

//	mGen >> mScope->bus( InputBus::SINE );
//	mNoise >> mScope->bus( InputBus::NOISE );

	mScope >> mGain >> ctx->getOutput();

	mNoise->stop();
	mGen->start();

	mEnableNoiseButton.setEnabled( false );
	mEnableSineButton.setEnabled( true );
}

void NodeTestApp::printDefaultOutput()
{
	audio2::DeviceRef device = audio2::Device::getDefaultOutput();

	CI_LOG_V( "device name: " << device->getName() );
	console() << "\t input channels: " << device->getNumInputChannels() << endl;
	console() << "\t output channels: " << device->getNumOutputChannels() << endl;
	console() << "\t samplerate: " << device->getSampleRate() << endl;
	console() << "\t frames per block: " << device->getFramesPerBlock() << endl;
}

void NodeTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.mBounds = Rectf( 0, 0, 200, 60 );
	mWidgets.push_back( &mPlayButton );

	mTestSelector.mSegments.push_back( "sine" );
	mTestSelector.mSegments.push_back( "2 to 1" );
	mTestSelector.mSegments.push_back( "1 to 2" );
	mTestSelector.mSegments.push_back( "funnel case" );
	mTestSelector.mSegments.push_back( "interleave pass-thru" );
	mTestSelector.mSegments.push_back( "auto-pulled" );
	mTestSelector.mBounds = Rectf( (float)getWindowWidth() * 0.67f, 0, (float)getWindowWidth(), 200 );
	mWidgets.push_back( &mTestSelector );

	float width = std::min( (float)getWindowWidth() - 20,  440.0f );
	Rectf sliderRect( getWindowCenter().x - width / 2, getWindowCenter().y + 10, getWindowCenter().x + width / 2, getWindowCenter().y + 50 );
	mGainSlider.mBounds = sliderRect;
	mGainSlider.mTitle = "Gain";
	mGainSlider.mMax = 1;
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
	mEnableNoiseButton.mBounds = mEnableSineButton.mBounds + Vec2f( 0, mEnableSineButton.mBounds.getHeight() + 10 );
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
	auto ctx = audio2::master();

	if( mPlayButton.hitTest( pos ) )
		ctx->setEnabled( ! ctx->isEnabled() );
	if( mGen && mEnableSineButton.hitTest( pos ) )
		mGen->setEnabled( ! mGen->isEnabled() );
	if( mNoise && mEnableNoiseButton.hitTest( pos ) ) // FIXME: this check doesn't work any more because there is always an mNoise / mGen
		mNoise->setEnabled( ! mNoise->isEnabled() );

	size_t currentIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		CI_LOG_V( "selected: " << currentTest );

		if( mScope )
			mScope->disconnectAll();

		if( currentTest == "sine" )
			setupGen();
		else if( currentTest == "2 to 1" )
			setup2to1();
		else if( currentTest == "1 to 2" )
			setup1to2();
		else if( currentTest == "interleave pass-thru" )
			setupInterleavedPassThru();
		else if( currentTest == "auto-pulled" )
			setupAutoPulled();
		else if( currentTest == "funnel case" )
			setupFunnelCase();

		ctx->printGraph();
	}
}

void NodeTestApp::draw()
{
	gl::clear();

	if( mScope && mScope->getNumConnectedInputs() ) {
		Vec2f padding( 20, 130 );
		Rectf scopeRect( padding.x, padding.y, getWindowWidth() - padding.x, getWindowHeight() - padding.y );
		drawAudioBuffer( mScope->getBuffer(), scopeRect, true );
	}

	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( NodeTestApp, RendererGl )
