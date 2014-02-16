/*
 Copyright (c) 2014, The Cinder Project

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

#include "cinder/audio2/Gen.h"
#include "cinder/audio2/Context.h"
#include "cinder/audio2/dsp/Dsp.h"
#include "cinder/audio2/Debug.h"
#include "cinder/Rand.h"

#define DEFAULT_WAVETABLE_SIZE 4096

using namespace ci;
using namespace std;

namespace cinder { namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - Gen
// ----------------------------------------------------------------------------------------------------

Gen::Gen( const Format &format )
	: NodeInput( format ), mFreq( this ), mPhase( 0 )
{
	mChannelMode = ChannelMode::SPECIFIED;
	setNumChannels( 1 );
}

void Gen::initialize()
{
	mSampleRate = (float)getContext()->getSampleRate();
}

// ----------------------------------------------------------------------------------------------------
// MARK: - GenNoise
// ----------------------------------------------------------------------------------------------------

void GenNoise::process( Buffer *buffer )
{
	float *data = buffer->getData();
	size_t count = buffer->getSize();

	for( size_t i = 0; i < count; i++ )
		data[i] = ci::randFloat( -1.0f, 1.0f );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - GenSine
// ----------------------------------------------------------------------------------------------------

void GenSine::process( Buffer *buffer )
{
	float *data = buffer->getData();
	const size_t count = buffer->getSize();
	const float phaseMul = float( 2 * M_PI / (double)mSampleRate );
	float phase = mPhase;

	if( mFreq.eval() ) {
		float *freqValues = mFreq.getValueArray();
		for( size_t i = 0; i < count; i++ ) {
			data[i] = math<float>::sin( phase );
			phase = fmodf( phase + freqValues[i] * phaseMul, float( M_PI * 2 ) );
		}
	}
	else {
		const float phaseIncr = mFreq.getValue() * phaseMul;
		for( size_t i = 0; i < count; i++ ) {
			data[i] = math<float>::sin( phase );
			phase = fmodf( phase + phaseIncr, float( M_PI * 2 ) );
		}
	}

	mPhase = phase;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - GenPhasor
// ----------------------------------------------------------------------------------------------------

void GenPhasor::process( Buffer *buffer )
{
	float *data = buffer->getData();
	const size_t count = buffer->getSize();
	const float phaseMul = 1.0f / mSampleRate;
	float phase = mPhase;

	if( mFreq.eval() ) {
		float *freqValues = mFreq.getValueArray();
		for( size_t i = 0; i < count; i++ ) {
			data[i] = phase;
			phase = fmodf( phase + freqValues[i] * phaseMul, 1 );
		}
	}
	else {
		const float phaseIncr = mFreq.getValue() * phaseMul;
		for( size_t i = 0; i < count; i++ ) {
			data[i] = phase;
			phase = fmodf( phase + phaseIncr, 1 );
		}
	}

	mPhase = phase;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - GenTriangle
// ----------------------------------------------------------------------------------------------------

namespace {

inline float calcTriangleSignal( float phase, float upSlope, float downSlope )
{
	// if up slope = down slope = 1, signal ranges from 0 to 0.5. so normalize this from -1 to 1
	float signal = std::min( phase * upSlope, ( 1 - phase ) * downSlope );
	return signal * 4 - 1;
}

} // anonymous namespace

void GenTriangle::process( Buffer *buffer )
{
	const float phaseMul = 1.0f / mSampleRate;
	float *data = buffer->getData();
	size_t count = buffer->getSize();
	float phase = mPhase;

	if( mFreq.eval() ) {
		float *freqValues = mFreq.getValueArray();
		for( size_t i = 0; i < count; i++ )	{
			data[i] = calcTriangleSignal( phase, mUpSlope, mDownSlope );
			phase = fmodf( phase + freqValues[i] * phaseMul, 1.0f );
		}

	}
	else {
		const float phaseIncr = mFreq.getValue() * phaseMul;
		for( size_t i = 0; i < count; i++ )	{
			data[i] = calcTriangleSignal( phase, mUpSlope, mDownSlope );
			phase = fmodf( phase + phaseIncr, 1 );
		}
	}

	mPhase = phase;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - GenWaveTable
// ----------------------------------------------------------------------------------------------------

GenWaveTable::GenWaveTable( const Format &format )
	: Gen( format ), mWaveformType( format.getWaveform() ), mTableSize( DEFAULT_WAVETABLE_SIZE ),
	mNumTables( 40 ), mReduceGibbs( true )
{
}

void GenWaveTable::initialize()
{
	Gen::initialize();

	setWaveform( mWaveformType );
}

void GenWaveTable::setWaveform( WaveformType type, size_t length )
{
	if( length )
		mTableSize = length;

	mWaveformType = type;
	fillTables();
}

void GenWaveTable::setGibbsReductionEnabled( bool b, bool reload )
{
	mReduceGibbs = b;
	if( reload )
		setWaveform( mWaveformType );
}

void GenWaveTable::setWaveformBandlimit( float hertz, bool reload )
{
	CI_ASSERT_MSG( 0, "not yet implemented" );

	//	float maxFreq = ;
//	setWaveformNumPartials( hertz / 440, reload );
}

/*
void GenWaveTable::setWaveformNumPartials( size_t numPartials, bool reload )
{
	mNumPartialCoeffs = numPartials;
	if( reload )
		setWaveform( mWaveformType );
}
*/

namespace {

#if 0

// truncate, phase range: 0-1
inline float tableLookup( float *table, size_t size, float phase )
{
	return table[(size_t)( phase * size )];
}

#else

// linear interpolation, phase range: 0-1
inline float tableLookup( float *table, size_t size, float phase )
{
	float lookup = phase * size;
	size_t index1 = (size_t)lookup;
	size_t index2 = ( index1 + 1 ) % size; // optimization: use boolean & operator instead
	float val1 = table[index1];
	float val2 = table[index2];
	float frac = lookup - (float)index1;

	return val2 + frac * ( val2 - val1 );
}

#endif

// gibbs effect reduction based on http://www.musicdsp.org/files/bandlimited.pdf
inline float calcGibbsReduceCoeff( size_t partial, size_t numPartials )
{
	float result = math<float>::cos( (float)partial * M_PI * 0.5f / numPartials );
	return result * result;
}

} // anonymous namespace

void GenWaveTable::process( Buffer *buffer )
{
	const size_t count = buffer->getSize();
	const float tableSize = mTableSize;
	const float numTables = mNumTables;
	const float nyquist = mSampleRate / 2.0f;
	const float samplePeriod = 1.0f / mSampleRate;
	float *data = buffer->getData();
	float phase = mPhase;

	if( mFreq.eval() ) {
		float *freqValues = mFreq.getValueArray();
		for( size_t i = 0; i < count; i++ ) {

			float f0 = freqValues[i];
			size_t tableIndex = min<size_t>( numTables - 1, floorf( nyquist / f0 ) - 1 );
			float *table = mTables[tableIndex].data();

			data[i] = tableLookup( table, tableSize, phase );
			phase = fmodf( phase + freqValues[i] * samplePeriod, 1 );
		}
	}
	else {
		// pick table based on the one with the most partials that won't alias
		float f0 = mFreq.getValue();
		size_t tableIndex = min<size_t>( numTables - 1, floorf( nyquist / f0 ) - 1 );
		float *table = mTables[tableIndex].data();

		const float phaseIncr = f0 * samplePeriod;
		for( size_t i = 0; i < count; i++ ) {
			data[i] = tableLookup( table, tableSize, phase );
			phase = fmodf( phase + phaseIncr, 1 );
		}
	}

	mPhase = phase;
}

// TODO: space partials based on human hearing
//	- web audio uses 3 per octave up to nyquist
void GenWaveTable::fillTables()
{
	CI_ASSERT( mNumTables > 0 );

	LOG_V( "filling " << mNumTables << " tables of size: " << mTableSize << "..." );

	if( mTables.size() != mNumTables )
		mTables.resize( mNumTables );

	for( size_t i = 0; i < mNumTables; i++ ) {
		auto &table = mTables[i];
		if( table.size() != mTableSize )
			table.resize( mTableSize );

		// naive partial spacing - add one per table (doesn't make much sense for square and triangle, since they only use odd partials
		fillBandLimitedTable( table.data(), i + 1 );
	}

	LOG_V( "..done" );
}

void GenWaveTable::fillBandLimitedTable( float *table, size_t numPartials )
{
	vector<float> partials;
	if( mWaveformType == SINE )
		partials.resize( 1 );
	else
		partials.resize( numPartials );

	switch( mWaveformType ) {
		case SINE:
			partials[0] = 1;
			break;
		case SQUARE:
			// 1 / x for odd x
			for( size_t x = 1; x <= partials.size(); x += 2 ) {
				float m = mReduceGibbs ? calcGibbsReduceCoeff( x, partials.size() ) : 1;
				partials[x - 1] = m / float( x );
			}
			break;
		case SAWTOOTH:
			// 1 / x
			for( size_t x = 1; x <= numPartials; x += 1 ) {
				float m = mReduceGibbs ? calcGibbsReduceCoeff( x, partials.size() ) : 1;
				partials[x - 1] = m / float( x );
			}
			break;
		case TRIANGLE: {
			// 1 / x^2 for odd x, alternating + and -
			float t = 1;
			for( size_t x = 1; x <= partials.size(); x += 2 ) {
				float m = mReduceGibbs ? calcGibbsReduceCoeff( x, partials.size() ) : t;
				partials[x - 1] = m / float( x * x );
				t *= -1;
			}
			break;
		}
		case PULSE:
			// TODO
		default:
			CI_ASSERT_NOT_REACHABLE();
	}

	dsp::sinesum( table, mTableSize, partials );
	dsp::normalize( table, mTableSize );
}

// TODO: consider using our own lock for updating the wavetable so that it can try and fail without blocking
void GenWaveTable::copyFromTable( float *array, size_t tableIndex ) const
{
	CI_ASSERT( tableIndex < mTables.size() );

	lock_guard<mutex> lock( getContext()->getMutex() );
	memcpy( array, mTables[tableIndex].data(), mTableSize * sizeof( float ) );
}

/*
void GenWaveTable::copyToTable( const float *array, size_t length )
{
	mWaveformType = CUSTOM;

	lock_guard<mutex> lock( getContext()->getMutex() );

	if( mTable.size() != length )
		mTable.resize( length );

	memcpy( mTable.data(), array, length * sizeof( float ) );
}
*/
} } // namespace cinder::audio2