#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "cinder/audio2/Gen.h"
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
	void prepareSettings( Settings *settings );
	void setup();
	void draw();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );
	void keyDown( KeyEvent event );

	audio2::GainRef				mGain;
	audio2::ScopeSpectralRef	mScope;
	audio2::GenWaveTableRef		mGen;

	audio2::BufferDynamic		mTableCopy;

	vector<TestWidget *>	mWidgets;
	Button					mPlayButton;
	VSelector				mTestSelector;
	HSlider					mGainSlider, mFreqSlider;
	TextInput				mNumPartialsInput, mTableSizeInput;
	SpectrumPlot			mSpectrumPlot;

};

void WaveTableTestApp::prepareSettings( Settings *settings )
{
	settings->setWindowSize( 1000, 800 );
}

void WaveTableTestApp::setup()
{
	auto ctx = audio2::Context::master();
	mGain = ctx->makeNode( new audio2::Gain );
	mGain->setValue( 0.0f );

//	mGen = ctx->makeNode( new audio2::GenWaveTable );
	mGen = ctx->makeNode( new audio2::GenWaveTable( audio2::GenWaveTable::Format().waveform( audio2::GenWaveTable::WaveformType::SAWTOOTH ) ) );
	mGen->setFreq( 100 );

	mScope = audio2::Context::master()->makeNode( new audio2::ScopeSpectral( audio2::ScopeSpectral::Format().windowSize( 2048 ) ) );
	mScope->setSmoothingFactor( 0 );

	mGen >> mScope >> mGain >> ctx->getOutput();

	ctx->printGraph();

	mTableCopy.setNumFrames( mGen->getTableSize() );
	mGen->copyFromTable( mTableCopy.getData() );

	setupUI();

	mGen->start();
}

void WaveTableTestApp::setupUI()
{
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.mBounds = Rectf( (float)getWindowWidth() - 200, 10, (float)getWindowWidth(), 60 );
	mWidgets.push_back( &mPlayButton );

	mTestSelector.mSegments.push_back( "sine" );
	mTestSelector.mSegments.push_back( "sawtooth" );
	mTestSelector.mSegments.push_back( "square" );
	mTestSelector.mSegments.push_back( "triangle" );
	mTestSelector.mSegments.push_back( "pulse" );
	mTestSelector.mSegments.push_back( "user" );
	mTestSelector.mBounds = Rectf( (float)getWindowWidth() - 200, mPlayButton.mBounds.y2 + 10, (float)getWindowWidth(), mPlayButton.mBounds.y2 + 190 );
	mWidgets.push_back( &mTestSelector );

	Rectf sliderRect = mTestSelector.mBounds;
	sliderRect.y1 = sliderRect.y2 + 10;
	sliderRect.y2 = sliderRect.y1 + 30;
	mGainSlider.mBounds = sliderRect;
	mGainSlider.mTitle = "gain";
	mGainSlider.mMax = 1;
	mGainSlider.set( mGain->getValue() );
	mWidgets.push_back( &mGainSlider );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mFreqSlider.mBounds = sliderRect;
	mFreqSlider.mTitle = "freq";
	mFreqSlider.mMax = 1200;
	mFreqSlider.set( mGen->getFreq() );
	mWidgets.push_back( &mFreqSlider );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 30 );
	mNumPartialsInput.mBounds = sliderRect;
	mNumPartialsInput.mTitle = "num partials";
//	mNumPartialsInput.setValue( mGen->getWaveformNumPartials() );
	mWidgets.push_back( &mNumPartialsInput );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 30 );
	mTableSizeInput.mBounds = sliderRect;
	mTableSizeInput.mTitle = "table size";
	mTableSizeInput.setValue( mGen->getTableSize() );
	mWidgets.push_back( &mTableSizeInput );

	getWindow()->getSignalMouseDown().connect( [this] ( MouseEvent &event ) { processTap( event.getPos() ); } );
	getWindow()->getSignalMouseDrag().connect( [this] ( MouseEvent &event ) { processDrag( event.getPos() ); } );
	getWindow()->getSignalTouchesBegan().connect( [this] ( TouchEvent &event ) { processTap( event.getTouches().front().getPos() ); } );
	getWindow()->getSignalTouchesMoved().connect( [this] ( TouchEvent &event ) {
		for( const TouchEvent::Touch &touch : getActiveTouches() )
			processDrag( touch.getPos() );
	} );

	mSpectrumPlot.setBorderColor( ColorA( 0, 0.9f, 0, 1 ) );

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
	else if( mNumPartialsInput.hitTest( pos ) ) {
	}
	else if( mTableSizeInput.hitTest( pos ) ) {
	}

	size_t currentIndex = mTestSelector.mCurrentSectionIndex;
	if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V( "selected: " << currentTest );

		if( currentTest == "sine" )
			mGen->setWaveform( audio2::GenWaveTable::WaveformType::SINE );
		else if( currentTest == "square" )
			mGen->setWaveform( audio2::GenWaveTable::WaveformType::SQUARE );
		else if( currentTest == "sawtooth" )
			mGen->setWaveform( audio2::GenWaveTable::WaveformType::SAWTOOTH );
		else if( currentTest == "triangle" )
			mGen->setWaveform( audio2::GenWaveTable::WaveformType::TRIANGLE );
		else if( currentTest == "pulse" )
			mGen->setWaveform( audio2::GenWaveTable::WaveformType::PULSE );

		mGen->copyFromTable( mTableCopy.getData() );
	}
}

void WaveTableTestApp::keyDown( KeyEvent event )
{
	TextInput *currentSelected = TextInput::getCurrentSelected();
	if( ! currentSelected )
		return;

	if( event.getCode() == KeyEvent::KEY_RETURN ) {
//		if( currentSelected == &mNumPartialsInput ) {
//			int numPartials = currentSelected->getValue();
//			LOG_V( "updating num partials from: " << mGen->getWaveformNumPartials() << " to: " << numPartials );
//			mGen->setWaveformNumPartials( numPartials, true );
//			mGen->copyFromTable( mTableCopy.getData() );
//		}
		if( currentSelected == &mTableSizeInput ) {
			int tableSize = currentSelected->getValue();
			LOG_V( "updating table size from: " << mGen->getTableSize() << " to: " << tableSize );
			mGen->setWaveform( mGen->getWaveForm(), tableSize );
			mTableCopy.setNumFrames( tableSize );
			mGen->copyFromTable( mTableCopy.getData() );
		}

	}
	else {
		if( event.getCode() == KeyEvent::KEY_BACKSPACE )
			currentSelected->processBackspace();
		else
			currentSelected->processChar( event.getChar() );
	}
}

void WaveTableTestApp::draw()
{
	gl::clear();

	const float padding = 10;
	const float scopeHeight = ( getWindowHeight() - padding * 4 ) / 3;

	Rectf rect( padding, padding, getWindowWidth() - padding - 200, scopeHeight + padding );
	drawAudioBuffer( mTableCopy, rect, true );

	rect += Vec2f( 0, scopeHeight + padding );
	drawAudioBuffer( mScope->getBuffer(), rect, true );

	rect += Vec2f( 0, scopeHeight + padding );
	mSpectrumPlot.setBounds( rect );
	mSpectrumPlot.draw( mScope->getMagSpectrum() );

	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( WaveTableTestApp, RendererGl )
