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

// TODO: append web audio's license

#pragma once

#include "audio2/Dsp.h"
#include "audio2/Buffer.h"

namespace cinder { namespace audio2 {

class Biquad {
public:
	Biquad();
    virtual ~Biquad();

	// frequency is 0 - 1 normalized, resonance and dbGain are in decibels.
    // Q is a unitless quality factor.
    void setLowpassParams( double cutoffFreq, double resonance );
    void setHighpassParams( double frequency, double resonance );
    void setBandpassParams( double frequency, double Q );
    void setLowShelfParams( double frequency, double dbGain );
    void setHighShelfParams( double frequency, double dbGain );
    void setPeakingParams( double frequency, double Q, double dbGain );
    void setAllpassParams( double frequency, double Q );
    void setNotchParams( double frequency, double Q );

	
	void process( const float *source, float *dest, size_t framesToProcess );

	//! Filter response at a set of n frequencies. The magnitude and phase response are returned in magResponse and phaseResponse. The phase response is in radians.
    void getFrequencyResponse( int nFrequencies, const float *frequency, float *magResponse, float *phaseResponse );

	//! Resets filter state
    void reset();

  private:
    void setNormalizedCoefficients( double b0, double b1, double b2, double a0, double a1, double a2 );

	// Filter coefficients. The filter is defined as
    //
    // y[n] + m_a1*y[n-1] + m_a2*y[n-2] = m_b0*x[n] + m_b1*x[n-1] + m_b2*x[n-2].
    double m_b0;
    double m_b1;
    double m_b2;
    double m_a1;
    double m_a2;


	// Filter memory
    double m_x1; // input delayed by 1 sample
    double m_x2; // input delayed by 2 samples
    double m_y1; // output delayed by 1 sample
    double m_y2; // output delayed by 2 samples

#if defined( CINDER_AUDIO_VDSP )
	void processVDsp( const float *source, float *dest, size_t framesToProcess );
    void processSliceVDsp( double *source, double *dest, double *coefficientsP, size_t framesToProcess );

	// used with vDSP only
//	AlignedArrayPtrd mInputBuffer, mOutputBuffer;
	std::vector<double> mInputBuffer, mOutputBuffer;
#endif

};

} } // namespace cinder::audio2