#pragma once

#include "audio2/Atomic.h"
#include "cinder/Rand.h"

#include <cmath>
#include <vector>

namespace audio2 {

	struct NoiseGen {
		NoiseGen( float amp = 0.0f ) : mAmp( amp )	{}

		void setAmp( float amp )	{ mAmp = amp; }

		void render( std::vector<float> *buffer )
		{
			float amp = mAmp;
			for( size_t i = 0; i < buffer->size(); i++ )
				buffer->at( i ) = ci::randFloat( -amp, amp );
		}

	private:
		std::atomic<float> mAmp;
	};

	struct SineGen {
		SineGen( size_t sampleRate = 0, float freq = 0.0f, float amp = 0.0f )
		: mSampleRate( sampleRate ), mFreq( freq ), mAmp( amp ), mPhase( 0.0f ), mPhaseIncr( 0.0f )
		{
				computePhaseIncr();
		}

		void setSampleRate( size_t sr )	{ mSampleRate = sr; computePhaseIncr(); }
		void setFreq( float freq )		{ mFreq = freq; computePhaseIncr(); }
		void setAmp( float amp )		{ mAmp = amp; }

		void render( std::vector<float> *buffer )
		{
			float amp = mAmp;
			for( size_t i = 0; i < buffer->size(); i++ ) {
				buffer->at( i ) = std::sin( mPhase ) * amp;
				mPhase += mPhaseIncr;
			}
		}

	private:
		void computePhaseIncr()
		{
			if( mSampleRate )
				mPhaseIncr = ( mFreq / (float)mSampleRate ) * 2.0f * M_PI;
		}
		std::atomic<float> mFreq, mAmp;
		size_t mSampleRate;
		float mPhase, mPhaseIncr;
	};


} // namespace audio2