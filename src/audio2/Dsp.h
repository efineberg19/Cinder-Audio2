#pragma once

#include "audio2/Atomic.h"
#include "audio2/assert.h"

#include "cinder/Rand.h"

#include <cmath>
#include <vector>

namespace audio2 {

struct UGen {
	UGen( size_t sampleRate = 0 ) : mSampleRate( sampleRate )	{}

	virtual void setSampleRate( size_t sr )	{ mSampleRate = sr; }
	virtual size_t getSampleRate() const	{ return mSampleRate; }

	virtual void process( float *channel, size_t count ) {}

	void process( std::vector<float> *channel )	{ process( channel->data(), channel->size() ); }

  protected:
	size_t mSampleRate;
};

struct NoiseGen : public UGen {
	NoiseGen( float amp = 0.0f ) : UGen(), mAmp( amp )	{}

	void setAmp( float amp )	{ mAmp = amp; }

	using UGen::process;
	void process( float *channel, size_t count ) override {
		float amp = mAmp;
		for( size_t i = 0; i < count; i++ )
			channel[i] = ci::randFloat( -amp, amp );
	}

  private:
	std::atomic<float> mAmp;
};

struct SineGen : public UGen {
	SineGen( float freq = 0.0f, float amp = 0.0f )
	: UGen(), mFreq( freq ), mAmp( amp ), mPhase( 0.0f ), mPhaseIncr( 0.0f )	{ computePhaseIncr(); }

	void setSampleRate( size_t sr )	{ mSampleRate = sr; computePhaseIncr(); }
	void setFreq( float freq )		{ mFreq = freq; computePhaseIncr(); }
	void setAmp( float amp )		{ mAmp = amp; }

	using UGen::process;
	void process( float *channel, size_t count ) override {
		float amp = mAmp;
		for( size_t i = 0; i < count; i++ ) {
			channel[i] = std::sin( mPhase ) * amp;
			mPhase += mPhaseIncr;
			if( mPhase > M_PI * 2.0 ) {
				mPhase -= (float)(M_PI * 2.0);
			}
		}
	}

  private:
	void computePhaseIncr()	{
		if( mSampleRate )
			mPhaseIncr = ( mFreq / (float)mSampleRate ) * 2.0f * (float)M_PI;
	}
	std::atomic<float> mFreq, mAmp;
	float mPhase, mPhaseIncr;
};

//! linear gain equal to -100db
const float kGainNegative100Decibels = 0.00001f;
const float kGainNegative100DecibelsInverse = 1.0f / kGainNegative100Decibels;

//! convert linear (0-1) gain to decibel (0-100) scale
inline float toDecibels( float gainLinear )
{
	if( gainLinear < kGainNegative100Decibels )
		return 0.0f;
	else
		return 20.0f * log10f( gainLinear * kGainNegative100DecibelsInverse );
}

inline float toLinear( float gainDecibels )
{
	if( gainDecibels < kGainNegative100Decibels )
		return 0.0f;
	else
		return( kGainNegative100Decibels * powf( 10.0f, gainDecibels * 0.05f ) );
}

inline size_t forcePow2( size_t val ) {
	if( val & ( val - 1 ) ) {
		size_t p = 1;
		while( p < val )
			p *= 2;
		return p;
	}
	return val;
}


} // namespace audio2