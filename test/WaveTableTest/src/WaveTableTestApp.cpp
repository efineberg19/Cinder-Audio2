#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "cinder/audio2/Gen.h"
#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/Scope.h"
#include "cinder/audio2/CinderAssert.h"
#include "cinder/audio2/Debug.h"

#include "cinder/audio2/Utilities.h"

#include "../../common/AudioTestGui.h"
#include "../../../samples/common/AudioDrawUtils.h"

using namespace ci;
using namespace ci::app;
using namespace std;


class WaveTableTestApp : public AppNative {
public:
	void prepareSettings( Settings *settings );
	void setup();
	void update();
	void draw();

	void setupUI();
	void processDrag( Vec2i pos );
	void processTap( Vec2i pos );
	void keyDown( KeyEvent event );

	void setupOsc();
	void setupPulse();

	audio2::GainRef				mGain;
	audio2::ScopeSpectralRef	mScope;
	audio2::GenOscillatorRef	mGenOsc;
	audio2::GenPulseRef			mGenPulse;
	audio2::GenRef				mGen;

	audio2::BufferDynamic		mTableCopy;

	vector<TestWidget *>	mWidgets;
	Button					mPlayButton;
	VSelector				mTestSelector;
	HSlider					mGainSlider, mFreqSlider, mFreqRampSlider, mPulseWidthSlider;
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
	mGain->setValue( 0.075f );

	mScope = audio2::Context::master()->makeNode( new audio2::ScopeSpectral( audio2::ScopeSpectral::Format().fftSize( 1024 ).windowSize( 2048 ) ) );
	mScope->setSmoothingFactor( 0 );

	setupOsc();
//	setupPulse();

	mGen >> mScope >> mGain >> ctx->getOutput();

	ctx->printGraph();

	setupUI();
}

void WaveTableTestApp::setupOsc()
{
	auto ctx = audio2::Context::master();

//	mGenOsc = ctx->makeNode( new audio2::GenOscillator );
	mGenOsc = ctx->makeNode( new audio2::GenOscillator( audio2::GenOscillator::Format().waveform( audio2::WaveformType::SAWTOOTH ) ) );
	mGenOsc->setFreq( 100 );
	mGenOsc->start();

	mGen = mGenOsc;
}

void WaveTableTestApp::setupPulse()
{
	if( ! mGenPulse ) {
		mGenPulse = audio2::Context::master()->makeNode( new audio2::GenPulse );
		mGenPulse->setFreq( mFreqSlider.mValueScaled );
		mGenPulse->start();
	}

	if( mGenOsc )
		mGenOsc->disconnectAll();

	mGenPulse >> mScope;
	mGen = mGenPulse;

#if 0
	// pwm
	auto sinMod = audio2::Context::master()->makeNode( new audio2::GenPhasor );
	sinMod->setFreq( 0.6f );
	sinMod->start();
	mGenPulse->getParamWidth()->setProcessor( sinMod );
#endif

	audio2::Context::master()->printGraph();
}

void WaveTableTestApp::setupUI()
{
	Rectf buttonRect( (float)getWindowWidth() - 200, 10, (float)getWindowWidth(), 60 );
	mPlayButton = Button( true, "stopped", "playing" );
	mPlayButton.mBounds = buttonRect;
	mWidgets.push_back( &mPlayButton );

	mTestSelector.mSegments.push_back( "sine" );
	mTestSelector.mSegments.push_back( "sawtooth" );
	mTestSelector.mSegments.push_back( "square" );
	mTestSelector.mSegments.push_back( "triangle" );
	mTestSelector.mSegments.push_back( "pulse" );
	mTestSelector.mSegments.push_back( "user" );
	mTestSelector.mBounds = Rectf( (float)getWindowWidth() - 200, buttonRect.y2 + 10, (float)getWindowWidth(), buttonRect.y2 + 190 );
	mWidgets.push_back( &mTestSelector );

	// freq slider is longer, along top
	mFreqSlider.mBounds = Rectf( 10, 10, getWindowWidth() - 210, 40 );
	mFreqSlider.mTitle = "freq";
	mFreqSlider.mMax = 5000;
//	mFreqSlider.mMin = 400;
//	mFreqSlider.mMax = 500;

	mFreqSlider.set( mGen->getFreq() );
	mWidgets.push_back( &mFreqSlider );

	Rectf sliderRect = mTestSelector.mBounds;
	sliderRect.y1 = sliderRect.y2 + 10;
	sliderRect.y2 = sliderRect.y1 + 30;
	mGainSlider.mBounds = sliderRect;
	mGainSlider.mTitle = "gain";
	mGainSlider.mMax = 1;
	mGainSlider.set( mGain->getValue() );
	mWidgets.push_back( &mGainSlider );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mFreqRampSlider.mBounds = sliderRect;
	mFreqRampSlider.mTitle = "freq ramp";
	mFreqRampSlider.mMax = 10;
	mFreqRampSlider.set( 4.00f );
	mWidgets.push_back( &mFreqRampSlider );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 10 );
	mPulseWidthSlider.mBounds = sliderRect;
	mPulseWidthSlider.mTitle = "pulse width";
	mPulseWidthSlider.set( 0.05f );
	mWidgets.push_back( &mPulseWidthSlider );


	sliderRect += Vec2f( 0, sliderRect.getHeight() + 30 );
	mNumPartialsInput.mBounds = sliderRect;
	mNumPartialsInput.mTitle = "num partials";
//	mNumPartialsInput.setValue( mGen->getWaveformNumPartials() );
	mWidgets.push_back( &mNumPartialsInput );

	sliderRect += Vec2f( 0, sliderRect.getHeight() + 30 );
	mTableSizeInput.mBounds = sliderRect;
	mTableSizeInput.mTitle = "table size";
	mTableSizeInput.setValue( mGenOsc->getTableSize() );
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
	else if( mFreqSlider.hitTest( pos ) )
		mGen->getParamFreq()->applyRamp( mFreqSlider.mValueScaled, mFreqRampSlider.mValueScaled );
	else if( mFreqRampSlider.hitTest( pos ) ) {
	}
	else if( mGenPulse && mPulseWidthSlider.hitTest( pos ) ) {
//		mGenPulse->setWidth( mPulseWidthSlider.mValueScaled );
		mGenPulse->getParamWidth()->applyRamp( mPulseWidthSlider.mValueScaled, 0.5f );
	}

}

void WaveTableTestApp::processTap( Vec2i pos )
{
	auto ctx = audio2::Context::master();
	size_t currentIndex = mTestSelector.mCurrentSectionIndex;

	if( mPlayButton.hitTest( pos ) )
		ctx->setEnabled( ! ctx->isEnabled() );
	else if( mNumPartialsInput.hitTest( pos ) ) {
	}
	else if( mTableSizeInput.hitTest( pos ) ) {
	}
	else if( mTestSelector.hitTest( pos ) && currentIndex != mTestSelector.mCurrentSectionIndex ) {
		string currentTest = mTestSelector.currentSection();
		LOG_V( "selected: " << currentTest );

		if( currentTest == "sine" )
			mGenOsc->setWaveform( audio2::WaveformType::SINE );
		else if( currentTest == "square" )
			mGenOsc->setWaveform( audio2::WaveformType::SQUARE );
		else if( currentTest == "sawtooth" )
			mGenOsc->setWaveform( audio2::WaveformType::SAWTOOTH );
		else if( currentTest == "triangle" )
			mGenOsc->setWaveform( audio2::WaveformType::TRIANGLE );
		else if( currentTest == "pulse" )
			setupPulse();

//		mGenOsc->getWaveTable()->copyTo( mTableCopy.getData() );
	}
	else
		processDrag( pos );
}

void WaveTableTestApp::keyDown( KeyEvent event )
{
	TextInput *currentSelected = TextInput::getCurrentSelected();
	if( ! currentSelected )
		return;

	if( event.getCode() == KeyEvent::KEY_RETURN ) {
//		if( currentSelected == &mTableSizeInput ) {
//			int tableSize = currentSelected->getValue();
//			LOG_V( "updating table size from: " << mGen->getTableSize() << " to: " << tableSize );
//			mGen->setWaveform( mGen->getWaveForm(), tableSize );
//			mTableCopy.setNumFrames( tableSize );
//			mGen->copyFromTable( mTableCopy.getData() );
//		}

	}
	else {
		if( event.getCode() == KeyEvent::KEY_BACKSPACE )
			currentSelected->processBackspace();
		else
			currentSelected->processChar( event.getChar() );
	}
}

void WaveTableTestApp::update()
{
	if( mGenOsc ) {
		mTableCopy.setNumFrames( mGenOsc->getTableSize() );
		mGenOsc->getWaveTable()->copyTo( mTableCopy.getData(), mGenOsc->getWaveTable()->calcBandlimitedTableIndex( mGenOsc->getFreq() ) );
	}
}

void WaveTableTestApp::draw()
{
	gl::clear();

	const float padding = 10;
	const float scopeHeight = ( getWindowHeight() - padding * 4 - mFreqSlider.mBounds.y2 ) / 3;

	Rectf rect( padding, padding + mFreqSlider.mBounds.y2, getWindowWidth() - padding - 200, mFreqSlider.mBounds.y2 + scopeHeight + padding );
	drawAudioBuffer( mTableCopy, rect, true );

	rect += Vec2f( 0, scopeHeight + padding );
	drawAudioBuffer( mScope->getBuffer(), rect, true );

	rect += Vec2f( 0, scopeHeight + padding );
	mSpectrumPlot.setBounds( rect );
	mSpectrumPlot.draw( mScope->getMagSpectrum() );

	drawWidgets( mWidgets );
}

CINDER_APP_NATIVE( WaveTableTestApp, RendererGl )
