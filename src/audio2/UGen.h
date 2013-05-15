#pragma once

#include "audio2/Atomic.h"
#include "cinder/Rand.h"

#include <cmath>
#include <vector>

namespace audio2 {

struct UGen {
	virtual void render( std::vector<float> *channel ) {}

	virtual void render( std::vector<std::vector<float> > *buffer )	{
		render( &buffer->at( 0 ) );
		for( size_t i = 1; i < buffer->size(); i++ )
			memcpy( buffer->at( i ).data(), buffer->at( 0 ).data(),  buffer->at( 0 ).size() * sizeof( float ) );
	}
};

struct NoiseGen : public UGen {
	NoiseGen( float amp = 0.0f ) : mAmp( amp )	{}

	void setAmp( float amp )	{ mAmp = amp; }

	using UGen::render;
	void render( std::vector<float> *channel ) override	{
		float amp = mAmp;
		for( size_t i = 0; i < channel->size(); i++ )
			channel->at( i ) = ci::randFloat( -amp, amp );
	}

  private:
	std::atomic<float> mAmp;
};

struct SineGen : public UGen {
	SineGen( size_t sampleRate = 0, float freq = 0.0f, float amp = 0.0f )
	: mSampleRate( sampleRate ), mFreq( freq ), mAmp( amp ), mPhase( 0.0f ), mPhaseIncr( 0.0f )	{ computePhaseIncr(); }

	void setSampleRate( size_t sr )	{ mSampleRate = sr; computePhaseIncr(); }
	void setFreq( float freq )		{ mFreq = freq; computePhaseIncr(); }
	void setAmp( float amp )		{ mAmp = amp; }

	using UGen::render;

	void render( std::vector<float> *channel ) override	{
		float amp = mAmp;
		for( size_t i = 0; i < channel->size(); i++ ) {
			channel->at( i ) = std::sin( mPhase ) * amp;
			mPhase += mPhaseIncr;
		}
	}

  private:
	void computePhaseIncr()	{
		if( mSampleRate )
			mPhaseIncr = ( mFreq / (float)mSampleRate ) * 2.0f * M_PI;
	}
	std::atomic<float> mFreq, mAmp;
	size_t mSampleRate;
	float mPhase, mPhaseIncr;
};

} // namespace audio2