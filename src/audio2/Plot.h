#pragma once

#include "cinder/Vector.h"
#include "cinder/PolyLine.h"
#include "cinder/TriMesh.h"

#include <vector>

namespace audio2 {

class Waveform {
  public:
	enum CalcMode { MinMax, Average };
    Waveform() {}
    Waveform( const std::vector<float> &samples, const ci::Vec2i &screenDimensions, int pixelsPerVertex = 2, CalcMode mode = MinMax );

    const ci::PolyLine2f& getOutline() { return mOutline; }
	const ci::TriMesh2d& getMesh();

    bool loaded() { return mOutline.getPoints().size() > 0; }
    
  private:
    ci::PolyLine2f mOutline;
	ci::TriMesh2d mMesh;
};

class WaveformPlot {
public:
	WaveformPlot() : mColorMinMax( ci::ColorA::gray( 0.5f ) ), mColorAvg( ci::ColorA::gray( 0.75f ) ) {}

	void load( const std::vector<float> &channels, const ci::Rectf &bounds, int pixelsPerVertex = 2 );
	void load( const std::vector<std::vector<float> > &channels, const ci::Rectf &bounds, int pixelsPerVertex = 2 );

	void setColorMinMax( const ci::ColorA &color ) { mColorMinMax = color; }
	void setColorAverage( const ci::ColorA &color ) { mColorAvg = color; }

	// TODO: this is gl specific - it should be elsewhere
	// - move it to freestanding gl::draw( WaveformPlot ) ?
	// - alternatively, load could return / fill a vector of TriMesh2d data, or some container, that can be drawn by user
	void drawGl();

private:
	std::vector<Waveform> mWaveforms;
	ci::ColorA mColorMinMax, mColorAvg;
	ci::Rectf mBounds;
};

} // namespace audio2
