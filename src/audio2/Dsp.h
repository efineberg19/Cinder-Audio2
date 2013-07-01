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

#pragma once

#include "audio2/Atomic.h"
#include "audio2/assert.h"

#include "cinder/Cinder.h"
#include "cinder/Rand.h"

#if defined( CINDER_COCOA )
	#define CINDER_AUDIO_DSP_ACCELERATE
	#include <Accelerate/Accelerate.h>
#endif

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

inline bool isPowerOf2( size_t val ) {
	return ( val & ( val - 1 ) ) == 0;
}

void generateBlackmanWindow( float *window, size_t length );
void generateHammWindow( float *window, size_t length );
void generateHannWindow( float *window, size_t length );

enum WindowType {
	BLACKMAN,
	HAMM,
	HANN,
	RECT		//! no window
};

//! fills \a window array with a windowing function specified by \a windowType
void generateWindow( WindowType windowType, float *window, size_t length );
//! fills \a audioData array with value \a value
void fill( float value, float *audioData, size_t length );
//! multiplies \a length elements of \a arrayA by \a arrayB and leaves the result at \a result.
void multiply( const float *arrayA, const float *arrayB, float *result, size_t length );
//! computes the Root-Mean-Squared value of \a audioData array
float rms( const float *audioData, size_t length );

} // namespace audio2