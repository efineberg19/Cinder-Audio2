#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/TextureFont.h"

#include "cinder/audio2/Context.h"
#include "cinder/audio2/Scope.h"
#include "cinder/audio2/Utilities.h"

#include "../../../samples/common/AudioDrawUtils.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class InputAnalyzer : public AppNative {
public:
	void setup();
	void mouseDown( MouseEvent event );
	void update();
	void draw();

	void drawLabels();
	void printBinInfo( float mouseX );

	audio2::LineInRef			mLineIn;
	audio2::ScopeSpectralRef	mScopeSpectral;
	vector<float>				mMagSpectrum;

	SpectrumPlot				mSpectrumPlot;
	gl::TextureFontRef			mTextureFont;
};

void InputAnalyzer::setup()
{
	auto ctx = audio2::Context::master();

	mLineIn = ctx->createLineIn();

	auto scopeFmt = audio2::ScopeSpectral::Format().fftSize( 2048 ).windowSize( 1024 );
	mScopeSpectral = ctx->makeNode( new audio2::ScopeSpectral( scopeFmt ) );

	mLineIn >> mScopeSpectral;

	mLineIn->start();
	ctx->start();
}

void InputAnalyzer::mouseDown( MouseEvent event )
{
	if( mSpectrumPlot.getBounds().contains( event.getPos() ) )
		printBinInfo( event.getX() );
}

void InputAnalyzer::update()
{
	mSpectrumPlot.setBounds( Rectf( 40, 40, getWindowWidth() - 40, getWindowHeight() - 40 ) );

	mMagSpectrum = mScopeSpectral->getMagSpectrum();
}

void InputAnalyzer::draw()
{
	gl::clear();
	gl::enableAlphaBlending();

	mSpectrumPlot.draw( mMagSpectrum );
	drawLabels();
}

void InputAnalyzer::drawLabels()
{
	if( ! mTextureFont )
		mTextureFont = gl::TextureFont::create( Font( Font::getDefault().getName(), 16 ) );

	gl::color( 0, 0.9f, 0.9f );

	// draw x-axis label
	string freqLabel = "Frequency (hertz)";
	mTextureFont->drawString( freqLabel, Vec2f( getWindowCenter().x - mTextureFont->measureString( freqLabel ).x / 2, getWindowHeight() - 20 ) );

	// draw y-axis label
	string dbLabel = "Magnitude (decibels)";
	gl::pushModelView();
		gl::translate( 30, getWindowCenter().y + mTextureFont->measureString( dbLabel ).x / 2 );
		gl::rotate( -90 );
		mTextureFont->drawString( dbLabel, Vec2f::zero() );
	gl::popModelView();
}

void InputAnalyzer::printBinInfo( float mouseX )
{
	size_t numBins = mScopeSpectral->getFftSize() / 2;
	size_t bin = min( numBins - 1, size_t( ( numBins * ( mouseX - mSpectrumPlot.getBounds().x1 ) ) / mSpectrumPlot.getBounds().getWidth() ) );

	float binFreqWidth = mScopeSpectral->getFreqForBin( 1 ) - mScopeSpectral->getFreqForBin( 0 );
	float freq = mScopeSpectral->getFreqForBin( bin );
	float mag = audio2::toDecibels( mMagSpectrum[bin] );

	console() << "bin: " << bin << ", freqency (hertz): " << freq << " - " << freq + binFreqWidth << ", magnitude (decibels): " << mag << endl;
}

CINDER_APP_NATIVE( InputAnalyzer, RendererGl )
