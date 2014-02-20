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

#pragma once

#include "cinder/audio2/WaveformType.h"

#include <vector>

namespace cinder { namespace audio2 { namespace dsp {

typedef std::shared_ptr<class WaveTable> WaveTableRef;

class WaveTable {
  public:
	WaveTable( size_t sampleRate, size_t tableSize = 0, size_t numTables = 0 );

	//! Adjusts the parameters effecting table size and calculate.
	//! \note This does not update the data, call fill() afterwards to refresh the table contents.
	void resize( size_t sampleRate, size_t tableSize = 0, size_t numTables = 0 );

	void fill( WaveformType type );

	const float*	getBandLimitedTable( float f0 ) const;

	void getBandLimitedTables( float f0, float **table1, float **table2, float* interpFactor );

	void copy( float *array, size_t tableIndex = 0 ) const;

	size_t getSampleRate() const { return mSampleRate; }
	size_t getTableSize() const	{ return mTableSize; }
	size_t getNumTables() const	{ return mNumTables; }

  protected:
	void		calcLimits();
	void		fillBandLimitedTable( WaveformType type, float *table, size_t numPartials );
	void		fillSinesum( float *array, size_t length, const std::vector<float> &partialCoeffs );
	size_t		getMaxHarmonicsForTable( size_t tableIndex ) const;

	size_t			mSampleRate, mTableSize, mNumTables;
	float			mMinMidiRange, mMaxMidiRange;

	std::vector<std::vector<float> >	mTables;
};

} } } // namespace cinder::audio2::dsp