#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "cinder/audio2/NodeInput.h"
#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/Scope.h"
#include "cinder/audio2/CinderAssert.h"
#include "cinder/audio2/Debug.h"

#include "../../common/AudioTestGui.h"
#include "../../../samples/common/AudioDrawUtils.h"

using namespace ci;
using namespace ci::app;
using namespace std;


class WaveTableTestApp : public AppNative {
public:
	void setup();
	void draw();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );

	audio2::GainRef				mGain;
	audio2::ScopeRef			mScope;
	audio2::GenWaveTableRef		mGen;

	audio2::BufferDynamic		mTableCopy;

	vector<TestWidget *>	mWidgets;
	Button					mPlayButton;
	VSelector				mTestSelector;
	HSlider					mGainSlider, mFreqSlider;

};

void WaveTableTestApp::setup()
{
	auto ctx = audio2::Context::master();
	mGain = ctx->makeNode( new audio2::Gain );
	mGain->setValue( 0.5f );

	mGen = ctx->makeNode( new audio2::GenWaveTable );
	mGen->setFreq( 440 );

	mTableCopy.setNumFrames( mGen->getTableSize() );
	mGen->copyFromTable( mTableCopy.getData() );

	mScope = audio2::Context::master()->makeNode( new audio2::Scope( audio2::Scope::Format().windowSize( 2048 ) ) );

	mGen >> mScope >> mGain >> ctx->getOutput();

	ctx->printGraph();

	setupUI();

	mGen->start();
}

void WaveTableTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.mBounds = Rectf( 0, 0, 200, 60 );
	mWidgets.push_back( &mPlayButton );

	mTestSelector.mSegments.push_back( "sine" );
	mTestSelector.mSegments.push_back( "sawtooth" );
	mTestSelector.mSegments.push_back( "square" );
	mTestSelector.mSegments.push_back( "triangle" );
	mTestSelector.mSegments.push_back( "user" );
	mTestSelector.mBounds = Rectf( (float)getWindowWidth() * 0.67f, 0, (float)getWindowWidth(), 180 );
	mWidgets.push_back( &mTestSelector );

//	float width = std::min( (float)getWindowWidth() - 20,  440.0f );
	Rectf sliderRect = mTestSelector.mBounds;
	sliderRect.y1 = sliderRect.y2 + 10;
	sliderRect.y2 = sliderRect.y1 + 30;
	mGainSlider.mBounds = sliderRect;
	mGainSlider.mTitle = "Gain";
	mGainSlider.mMax = 1;
	mGainSlider.set( mGain->getValue() );
	mWidgets.push_back( &mGainSlider );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mFreqSlider.mBounds = sliderRect;
	mFreqSlider.mTitle = "Freq";
	mFreqSlider.mMax = 800;
	mFreqSlider.set( mGen->getFreq() );
	mWidgets.push_back( &mFreqSlider );

	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalMouseDrag().connect( [this] ( MouseEvent &event ) { processDrag( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );
	getWindow()->getSignalTouchesMoved().connect( [this] ( TouchEvent &event ) {
		for( const TouchEvent::Touch &touch : getActiveTouches() )
			processDrag( touch.getPos() );
	} );

	gl::enableAlphaBlending();
}

void WaveTableTestApp::processDrag( Vec2i pos )
{
	if( mGainSlider.hitTest( pos ) )
		mGain->getParam()->applyRamp( mGainSlider.mValueScaled, 0.03f );
	if( mFreqSlider.hitTest( pos ) )
		mGen->getParamFreq()->applyRamp( mFreqSlider.mValueScaled, 0.03f );
}

void WaveTableTestApp::processTap( Vec2i pos )
{
	auto ctx = audio2::Context::master();

	if( mPlayButton.hitTest( pos ) )
		ctx->setEnabled( ! ctx->isEnabled() );

	size_t currentIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V( "selected: " << currentTest );

//		if( currentTest == "sine" )
//			setupGen();
//		else if( currentTest == "2 to 1" )
//			setup2to1();
//		else if( currentTest == "1 to 2" )
//			setup1to2();
//		else if( currentTest == "interleave pass-thru" )
//			setupInterleavedPassThru();
//		else if( currentTest == "auto-pulled" )
//			setupAutoPulled();
	}
}

void WaveTableTestApp::draw()
{
	gl::clear();

	const float padding = 20;
	Rectf scopeRect( padding, padding, getWindowWidth() - padding, getWindowHeight() / 2 - padding );

	drawAudioBuffer( mTableCopy, scopeRect, true );

	scopeRect += Vec2f( 0, getWindowHeight() / 2 );
	drawAudioBuffer( mScope->getBuffer(), scopeRect, true );

	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( WaveTableTestApp, RendererGl )
