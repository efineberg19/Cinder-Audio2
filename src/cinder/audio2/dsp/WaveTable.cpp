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

#include "cinder/audio2/dsp/WaveTable.h"
#include "cinder/audio2/dsp/Dsp.h"
#include "cinder/audio2/Utilities.h"
#include "cinder/audio2/Debug.h"
#include "cinder/CinderMath.h"

#include "cinder/Timer.h" // TEMP

#define DEFAULT_WAVETABLE_SIZE 4096
#define DEFAULT_NUM_WAVETABLES 40

using namespace std;

namespace {

// gibbs effect reduction based on http://www.musicdsp.org/files/bandlimited.pdf
inline float calcGibbsReduceCoeff( size_t partial, size_t numPartials )
{
	if( numPartials <= 1 )
		return 1;

	float result = ci::math<float>::cos( (float)partial * M_PI * 0.5f / numPartials );
	return result * result;
}
	
} // anonymous namespace

namespace cinder { namespace audio2 { namespace dsp {

WaveTable::WaveTable( size_t sampleRate, size_t tableSize, size_t numTables )
	: mSampleRate( sampleRate )
{
	mTableSize = tableSize ? tableSize : DEFAULT_WAVETABLE_SIZE;
	mNumTables = numTables ? numTables : DEFAULT_NUM_WAVETABLES;

	calcLimits();
}

void WaveTable::resize( size_t sampleRate, size_t tableSize, size_t numTables )
{
	mSampleRate = sampleRate;
	
	if( tableSize )
		mTableSize = tableSize;
	if( numTables )
		mNumTables = numTables;
}

void WaveTable::fill( WaveformType type )
{
	LOG_V( "filling " << mNumTables << " tables of size: " << mTableSize << "..." );
	Timer timer( true );

	if( mTables.size() != mNumTables )
		mTables.resize( mNumTables );

	for( size_t i = 0; i < mNumTables; i++ ) {
		auto &table = mTables[i];
		if( table.size() != mTableSize )
			table.resize( mTableSize );

		// last table always has only one partial
		if( i == mNumTables - 1 ) {
			fillBandLimitedTable( type, table.data(), 1 );
			LOG_V( "\t[" << i << "] LAST, nyquist / 4 and above, max partials: 1 " );
			break;
		}

		size_t maxPartialsForFreq = getMaxHarmonicsForTable( i );
		fillBandLimitedTable( type, table.data(), maxPartialsForFreq );
	}

	LOG_V( "..done, seconds: " << timer.getSeconds() );
}

// note: for at least sawtooth and square, this must be recomputed for every table so that gibbs reduction is accurate
void WaveTable::fillBandLimitedTable( WaveformType type, float *table, size_t numPartials )
{
	vector<float> partials;
	if( type == WaveformType::SINE )
		partials.resize( 1 );
	else
		partials.resize( numPartials );

	switch( type ) {
		case WaveformType::SINE:
			partials[0] = 1;
			break;
		case WaveformType::SQUARE:
			// 1 / x for odd x
			for( size_t x = 1; x <= partials.size(); x += 2 ) {
				float m = calcGibbsReduceCoeff( x, partials.size() );
				partials[x - 1] = m / float( x );
			}
			break;
		case WaveformType::SAWTOOTH:
			// 1 / x
			for( size_t x = 1; x <= numPartials; x += 1 ) {
				float m = calcGibbsReduceCoeff( x, partials.size() );
				partials[x - 1] = m / float( x );
			}
			break;
		case WaveformType::TRIANGLE: {
			// 1 / x^2 for odd x, alternating + and -
			float t = 1;
			for( size_t x = 1; x <= partials.size(); x += 2 ) {
				partials[x - 1] = t / float( x * x );
				t *= -1;
			}
			break;
		}
//		case PULSE:
			// TODO: try making pulse with offset two sawtooth sinesums
			//	- is this worth it? if you just use one sawtooth and index it twice, can do pulse width modulation
			//	- this would be easier if there were a WaveTable class and a custom Gen could do the indexing twice, rather than muddying this Gen's process()
		default:
			CI_ASSERT_NOT_REACHABLE();
	}

	fillSinesum( table, mTableSize, partials );
	dsp::normalize( table, mTableSize );
}

void WaveTable::fillSinesum( float *array, size_t length, const std::vector<float> &partials )
{
	memset( array, 0, length * sizeof( float ) );

	double phase = 0;
	const double phaseIncr = ( 2.0 * M_PI ) / (double)length;

	for( size_t i = 0; i < length; i++ ) {
		double partialPhase = phase;
		for( size_t p = 0; p < partials.size(); p++ ) {
			array[i] += partials[p] * math<float>::sin( partialPhase );
			partialPhase += phase;
		}
		
		phase += phaseIncr;
	}
}

size_t WaveTable::getMaxHarmonicsForTable( size_t tableIndex ) const
{
	const float nyquist = (float)mSampleRate / 2.0f;
	const float midiRangePerTable = ( mMaxMidiRange - mMinMidiRange ) / ( mNumTables - 1 );
	const float maxMidi = mMinMidiRange + tableIndex * midiRangePerTable;
	const float maxF0 = toFreq( maxMidi );

	size_t maxPartialsForFreq( nyquist / maxF0 );

	LOG_V( "\t[" << tableIndex << "] midi: " << maxMidi << ", max f0: " << maxF0 << ", max partials: " << maxPartialsForFreq );
	return maxPartialsForFreq;
}

const float* WaveTable::getBandLimitedTable( float f0 ) const
{
	CI_ASSERT_MSG( f0 >= 0, "negative frequencies not yet handled" ); // TODO: negate in GenWaveTable

	const float f0Midi = toMidi( f0 );

	if( f0Midi <= mMinMidiRange )
		return mTables.front().data();
	else if( f0Midi >= mMaxMidiRange )
		return mTables.back().data();

	const float midiRangePerTable = ( mMaxMidiRange - mMinMidiRange ) / ( mNumTables - 1 );
	const float maxMidi = f0Midi;

	size_t tableIndex = 1 + ( maxMidi - mMinMidiRange ) / midiRangePerTable;
	return mTables[tableIndex].data();
}

void WaveTable::copy( float *array, size_t tableIndex ) const
{
	CI_ASSERT( tableIndex < mTables.size() );

	memcpy( array, mTables[tableIndex].data(), mTableSize * sizeof( float ) );
}

void WaveTable::calcLimits()
{
	mMinMidiRange = toMidi( 20 );
	mMaxMidiRange = toMidi( (float)mSampleRate / 4.0f ); // everything above can only have one partial
}

} } } // namespace cinder::audio2::dsp