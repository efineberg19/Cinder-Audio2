/*
 Copyright (c) 2013, The Cinder Project

 This code is intended to be used with the Cinder C++ library, http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#include "Plot.h"

#include "cinder/CinderMath.h"
#include "cinder/Triangulate.h"
#include "cinder/gl/gl.h"

using namespace std;
using namespace ci;
using namespace ci::audio2;

// ----------------------------------------------------------------------------------------------------
// MARK: - WaveformPlot
// ----------------------------------------------------------------------------------------------------

inline void calcMinMaxForSection( const float *buffer, size_t samplesPerSection, float &max, float &min ) {
	max = 0.0f;
	min = 0.0f;
	for( size_t k = 0; k < samplesPerSection; k++ ) {
		float s = buffer[k];
		max = math<float>::max( max, s );
		min = math<float>::min( min, s );
	}
}

inline void calcAverageForSection( const float *buffer, size_t samplesPerSection, float &upper, float &lower ) {
	upper = 0.0f;
	lower = 0.0f;
	for( size_t k = 0; k < samplesPerSection; k++ ) {
		float s = buffer[k];
		if( s > 0.0f ) {
			upper += s;
		} else {
			lower += s;
		}
	}
	upper /= samplesPerSection;
	lower /= samplesPerSection;
}

void Waveform::load( const float *samples, size_t numSamples, const ci::Vec2i &waveSize, size_t pixelsPerVertex, CalcMode mode )
{
    float height = waveSize.y / 2.0f;
    size_t numSections = waveSize.x / pixelsPerVertex + 1;
    size_t samplesPerSection = numSamples / numSections;

	vector<Vec2f> &points = mOutline.getPoints();
	points.resize( numSections * 2 );

    for( int i = 0; i < numSections; i++ ) {
		float x = i * pixelsPerVertex;
		float yUpper, yLower;
		if( mode == CalcMode::MIN_MAX ) {
			calcMinMaxForSection( &samples[i * samplesPerSection], samplesPerSection, yUpper, yLower );
		} else {
			calcAverageForSection( &samples[i * samplesPerSection], samplesPerSection, yUpper, yLower );
		}
		points[i] = Vec2f( x, height - height * yUpper );
		points[numSections * 2 - i - 1] = Vec2f( x, height - height * yLower );
    }
	mOutline.setClosed();

	mMesh = Triangulator( mOutline ).calcMesh();
}


void WaveformPlot::load( const std::vector<float> &samples, const ci::Rectf &bounds, size_t pixelsPerVertex )
{
	mBounds = bounds;
	mWaveforms.clear();

	Vec2i waveSize = bounds.getSize();
	mWaveforms.push_back( Waveform( samples, waveSize, pixelsPerVertex, Waveform::CalcMode::MIN_MAX ) );
	mWaveforms.push_back( Waveform( samples, waveSize, pixelsPerVertex, Waveform::CalcMode::AVERAGE ) );
}

void WaveformPlot::load( const BufferRef &buffer, const ci::Rectf &bounds, size_t pixelsPerVertex )
{
	mBounds = bounds;
	mWaveforms.clear();

	size_t numChannels = buffer->getNumChannels();
	Vec2i waveSize = bounds.getSize();
	waveSize.y /= numChannels;
	for( size_t ch = 0; ch < numChannels; ch++ ) {
		mWaveforms.push_back( Waveform( buffer->getChannel( ch ), buffer->getNumFrames(), waveSize, pixelsPerVertex, Waveform::CalcMode::MIN_MAX ) );
		mWaveforms.push_back( Waveform( buffer->getChannel( ch ), buffer->getNumFrames(), waveSize, pixelsPerVertex, Waveform::CalcMode::AVERAGE ) );
	}
}

void WaveformPlot::draw()
{
	auto &waveforms = getWaveforms();
	if( waveforms.empty() ) {
		return;
	}

	gl::color( mColorMinMax );
	gl::draw( waveforms[0].getMesh() );

	gl::color( mColorAverage );
	gl::draw( waveforms[1].getMesh() );

	if( waveforms.size() > 2 ) {
		gl::pushMatrices();
		gl::translate( 0.0f, getBounds().getHeight() / 2 );

		gl::color( mColorMinMax );
		gl::draw( waveforms[2].getMesh() );

		gl::color( mColorAverage );
		gl::draw( waveforms[3].getMesh() );
		
		gl::popMatrices();
	}
}

// ----------------------------------------------------------------------------------------------------
// MARK: - SpectrumPlot
// ----------------------------------------------------------------------------------------------------

void SpectrumPlot::draw( const vector<float> &magSpectrum )
{
	if( magSpectrum.empty() )
		return;

	Color bottomColor( 0.0f, 0.0f, 0.7f );

	float width = mBounds.getWidth();
	float height = mBounds.getHeight();
	size_t numBins = magSpectrum.size();
	float padding = 0.0f;
	float binWidth = ( width - padding * ( numBins - 1 ) ) / (float)numBins;

	size_t numVerts = magSpectrum.size() * 2 + 2;
	if( mVerts.size() < numVerts ) {
		mVerts.resize( numVerts );
		mColors.resize( numVerts );
	}

	size_t currVertex = 0;
	Rectf bin( mBounds.x1, mBounds.y1, mBounds.x1 + binWidth, mBounds.y2 );
	for( size_t i = 0; i < numBins; i++ ) {
		float m = magSpectrum[i];
		if( mScaleDecibels )
			m = toDecibels( m ) / 100.0f;

		bin.y1 = bin.y2 - m * height;

		mVerts[currVertex] = bin.getLowerLeft();
		mColors[currVertex] = bottomColor;
		mVerts[currVertex + 1] = bin.getUpperLeft();
		mColors[currVertex + 1] = Color( 0.0f, m, 0.7f );

		bin += Vec2f( binWidth + padding, 0.0f );
		currVertex += 2;
	}

	mVerts[currVertex] = bin.getLowerLeft();
	mColors[currVertex] = bottomColor;
	mVerts[currVertex + 1] = bin.getUpperLeft();
	mColors[currVertex + 1] = mColors[currVertex - 1];

	gl::color( 0.0f, 0.9f, 0.0f );

	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_COLOR_ARRAY );
	glVertexPointer( 2, GL_FLOAT, 0, mVerts.data() );
	glColorPointer( 3, GL_FLOAT, 0, mColors.data() );
	glDrawArrays( GL_TRIANGLE_STRIP, 0, (GLsizei)mVerts.size() );
	glDisableClientState( GL_VERTEX_ARRAY );
	glDisableClientState( GL_COLOR_ARRAY );
}

