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

    const ci::PolyLine2f& getOutline() const	{ return mOutline; }
	const ci::TriMesh2d& getMesh() const		{ return mMesh; };

    bool loaded() { return mOutline.getPoints().size() > 0; }
    
  private:
    ci::PolyLine2f mOutline;
	ci::TriMesh2d mMesh;
};

class WaveformPlot {
public:
	WaveformPlot()	{}

	void load( const std::vector<float> &channels, const ci::Rectf &bounds, int pixelsPerVertex = 2 );
	void load( const std::vector<std::vector<float> > &channels, const ci::Rectf &bounds, int pixelsPerVertex = 2 );

	const std::vector<Waveform>& getWaveforms() const	{ return mWaveforms; }
	const ci::Rectf& getBounds() const					{ return mBounds; }

private:
	std::vector<Waveform> mWaveforms;
	ci::Rectf mBounds;
};

} // namespace audio2

namespace cinder { namespace gl {

	void draw( const audio2::WaveformPlot &plot, const Vec2i &offset = Vec2i::zero(), float scale = 1.0f, const ColorA &colorMinMax = ColorA::gray( 0.5f ), const ColorA &colorAverage = ColorA::gray( 0.75f ) );

} } // namespace cinder::gl

