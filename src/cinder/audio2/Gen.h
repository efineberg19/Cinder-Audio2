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

#include "cinder/audio2/NodeInput.h"

namespace cinder { namespace audio2 {

typedef std::shared_ptr<class Gen>						GenRef;
typedef std::shared_ptr<class GenWaveTable>				GenWaveTableRef;

//! Base class for NodeInput's that generate audio samples.
class Gen : public NodeInput {
public:
	Gen( const Format &format = Format() );

	void initialize() override;

	void setFreq( float freq )		{ mFreq.setValue( freq ); }
	float getFreq() const			{ return mFreq.getValue(); }

	Param* getParamFreq()			{ return &mFreq; }

protected:
	float mSampleRate;

	Param mFreq;
	float mPhase;
};

//! Noise generator. \note freq param is ignored
class GenNoise : public Gen {
public:
	GenNoise( const Format &format = Format() ) : Gen( format ) {}

	void process( Buffer *buffer ) override;
};

//! Phase generator, i.e. sawtooth waveform that runs from 0 to 1.
class GenPhasor : public Gen {
public:
	GenPhasor( const Format &format = Format() ) : Gen( format )
	{}

	void process( Buffer *buffer ) override;
};

//! Sine waveform generator.
class GenSine : public Gen {
public:
	GenSine( const Format &format = Format() ) : Gen( format )
	{}

	void process( Buffer *buffer ) override;
};

//! Triangle waveform generator.
class GenTriangle : public Gen {
public:
	GenTriangle( const Format &format = Format() ) : Gen( format ), mUpSlope( 1 ), mDownSlope( 1 )
	{}

	void setUpSlope( float up )			{ mUpSlope = up; }
	void setDownSlope( float down )		{ mDownSlope = down; }

	float getUpSlope() const		{ return mUpSlope; }
	float getDownSlope() const		{ return mDownSlope; }

	void process( Buffer *buffer ) override;

private:
	std::atomic<float> mUpSlope, mDownSlope;
};

//! Generator that uses wavetable lookup.
class GenWaveTable : public Gen {
public:
	enum WaveformType { SINE, SQUARE, SAWTOOTH, TRIANGLE, PULSE, CUSTOM };

	struct Format : public Node::Format {
		Format() : mWaveformType( SINE )	{}

		Format&		waveform( WaveformType type )	{ mWaveformType = type; return *this; }

		const WaveformType& getWaveform() const	{ return mWaveformType; }

	private:
		WaveformType mWaveformType;
	};

	GenWaveTable( const Format &format = Format() );

	void initialize() override;
	void process( Buffer *buffer ) override;


	//! Fills the internal table with a waveform of type \a type, with optional \a length (default = 512 or the table's current size).
	//! The waveform is created using band-limited additive synthesis in order to avoid foldover (aliasing). \see setWaveformBandlimit
	//! \note WaveformType CUSTOM does nothing here, it is used when filling the .
	void setWaveform( WaveformType type, size_t length = 0 );
	//! Sets the maximum frequency for partial coefficients when creating bandlimited waveforms. Default is the current nyqyst (Context's samplerate / 2) - 4k hertz.
	void setWaveformBandlimit( float hertz, bool reload = false );
	//! Sets the number of partials used when creating bandlimited waveforms. TODO: document default
	void setWaveformNumPartials( size_t numPartials, bool reload = false );

	size_t			getWaveformNumPartials() const	{ return mNumPartialCoeffs; }
	WaveformType	getWaveForm() const				{ return mWaveformType; }

	size_t getTableSize() const	{ return mTable.size(); }

	void copyFromTable( float *array ) const;
	void copyToTable( const float *array, size_t length = 0 );

protected:
	void fillTableImpl( WaveformType type );

	size_t				mNumPartialCoeffs;
	WaveformType		mWaveformType;
	std::vector<float>	mTable;
};

} } // namespace cinder::audio2