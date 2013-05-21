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

	virtual void render( std::vector<float> *channel ) {}

	virtual void render( std::vector<std::vector<float> > *buffer )	{
		render( &buffer->at( 0 ) );
		for( size_t i = 1; i < buffer->size(); i++ )
			memcpy( buffer->at( i ).data(), buffer->at( 0 ).data(),  buffer->at( 0 ).size() * sizeof( float ) );
	}

  protected:
	size_t mSampleRate;
};

struct NoiseGen : public UGen {
	NoiseGen( float amp = 0.0f ) : UGen(), mAmp( amp )	{}

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
	SineGen( float freq = 0.0f, float amp = 0.0f )
	: UGen(), mFreq( freq ), mAmp( amp ), mPhase( 0.0f ), mPhaseIncr( 0.0f )	{ computePhaseIncr(); }

	void setSampleRate( size_t sr )	{ mSampleRate = sr; computePhaseIncr(); }
	void setFreq( float freq )		{ mFreq = freq; computePhaseIncr(); }
	void setAmp( float amp )		{ mAmp = amp; }

	using UGen::render;

	void render( std::vector<float> *channel ) override	{
		float amp = mAmp;
		for( size_t i = 0; i < channel->size(); i++ ) {
			channel->at( i ) = std::sin( mPhase ) * amp;
			mPhase += mPhaseIncr;
			if( mPhase > M_PI * 2.0f ) {
				mPhase -= M_PI * 2.0f;
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

inline void interleaveStereoBuffer( BufferT *buffer, ChannelT *interleaved )
{
	CI_ASSERT( buffer->size() == 2 );

	ChannelT &left = (*buffer)[0];
	ChannelT &right = (*buffer)[1];
	float *mixed = interleaved->data();

	size_t blockSize = left.size();
	size_t i, j;
	for( i = 0, j = 0; i < blockSize; i++, j += 2 ) {
		mixed[j] = left[i];
		mixed[j + 1] = right[i];
	}
}

inline void deinterleaveStereoBuffer( ChannelT *interleaved, BufferT *buffer )
{
	CI_ASSERT( buffer->size() == 2 );

	ChannelT &left = (*buffer)[0];
	ChannelT &right = (*buffer)[1];
	float *mixed = interleaved->data();

	size_t blockSize = left.size();
	size_t i, j;
	for( i = 0, j = 0; i < blockSize; i++, j += 2 ) {
		left[i] = mixed[j];
		right[i] = mixed[j + 1];
	}
}

} // namespace audio2