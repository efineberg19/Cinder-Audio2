#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"

#include "cinder/audio2/Context.h"
#include "cinder/audio2/Scope.h"

#include "../../../samples/common/AudioDrawUtils.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class InputAnalyzer : public AppNative {
public:
	void setup();
	void mouseDrag( MouseEvent event ) override;
	void draw();

	audio2::LineInRef			mLineIn;
	audio2::ScopeSpectralRef	mScopeSpectral;

	SpectrumPlot				mSpectrumPlot;
};

void InputAnalyzer::setup()
{
	auto ctx = audio2::Context::master();

	mLineIn = ctx->createLineIn();

	auto scopeFmt = audio2::ScopeSpectral::Format().fftSize( 2048 ).windowSize( 1024 );
	mScopeSpectral = ctx->makeNode( new audio2::ScopeSpectral( scopeFmt ) );

	mLineIn >> mScopeSpectral;

	ctx->printGraph();

	// not currently needed since LineIn is auto-enabled by default.
	//	- Is this inconsistent? Other NodeInput's default to needing start().
//	mLineIn->start();

	ctx->start();
}

void InputAnalyzer::mouseDrag( MouseEvent event )
{
}

void InputAnalyzer::draw()
{
	gl::clear( Color() );

	const auto &mag = mScopeSpectral->getMagSpectrum();
	mSpectrumPlot.setBounds( Rectf( 30, 30, getWindowWidth() - 30, getWindowHeight() - 30 ) );
	mSpectrumPlot.draw( mag );
}

CINDER_APP_NATIVE( InputAnalyzer, RendererGl )
