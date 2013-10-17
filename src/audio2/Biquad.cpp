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

#include "audio2/Biquad.h"
#include "audio2/Buffer.h"

#include "cinder/Cinder.h"

#if defined( CINDER_AUDIO_VDSP )
	#include <Accelerate/Accelerate.h>
#endif
//
namespace cinder { namespace audio2 {

namespace {

const size_t kBufferSize = 1024;

// This is a nop if we can flush denormals to zero in hardware.
static inline float flushDenormalFloatToZero(float f)
{
#if defined( CINDER_MSW ) && ( ! _M_IX86_FP )
	// For systems using x87 instead of sse, there's no hardware support to flush denormals automatically. Hence, we need to flush denormals to zero manually.
	return (fabs(f) < FLT_MIN) ? 0.0f : f;
#else
	return f;
#endif
}

}

Biquad::Biquad()
{
#if defined( CINDER_AUDIO_VDSP )
//	mInputBuffer = makeAlignedArray<double>( kBufferSize + 2 );
//	mOutputBuffer = makeAlignedArray<double>( kBufferSize + 2 );
#endif // defined( CINDER_AUDIO_VDSP )

	// Initialize as pass-thru (straight-wire, no filter effect)
    setNormalizedCoefficients( 1, 0, 0, 1, 0, 0 );

    reset(); // clear filter memory
}

Biquad::~Biquad()
{
}

void Biquad::process( const float* source, float* dest, size_t framesToProcess )
{
    size_t n = framesToProcess;

    // Create local copies of member variables
    double x1 = m_x1;
    double x2 = m_x2;
    double y1 = m_y1;
    double y2 = m_y2;

    double b0 = m_b0;
    double b1 = m_b1;
    double b2 = m_b2;
    double a1 = m_a1;
    double a2 = m_a2;

    while (n--) {
        // FIXME: this can be optimized by pipelining the multiply adds...
        float x = *source++;
        float y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;

        *dest++ = y;

        // Update state variables
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
    }

    // Local variables back to member. Flush denormals here so we don't slow down the inner loop above.
    m_x1 = flushDenormalFloatToZero( x1 );
    m_x2 = flushDenormalFloatToZero( x2 );
    m_y1 = flushDenormalFloatToZero( y1 );
    m_y2 = flushDenormalFloatToZero( y2 );

    m_b0 = b0;
    m_b1 = b1;
    m_b2 = b2;
    m_a1 = a1;
    m_a2 = a2;
}

void Biquad::getFrequencyResponse( int nFrequencies, const float *frequency, float *magResponse, float *phaseResponse )
{

}

void Biquad::setLowpassParams( double cutoffFreq, double resonance )
{
	// Limit cutoff to 0 to 1.
	double cutoff = std::max( 0.0, std::min( cutoffFreq, 1.0 ) );

	if (cutoff == 1) {
		// When cutoff is 1, the z-transform is 1.
		setNormalizedCoefficients(1, 0, 0, 1, 0, 0);
	} else if (cutoff > 0) {
		// Compute biquad coefficients for lowpass filter
		resonance = std::max(0.0, resonance); // can't go negative
		double g = pow(10.0, 0.05 * resonance);
		double d = sqrt((4 - sqrt(16 - 16 / (g * g))) / 2);

		double theta = M_PI * cutoff;
		double sn = 0.5 * d * sin(theta);
		double beta = 0.5 * (1 - sn) / (1 + sn);
		double gamma = (0.5 + beta) * cos(theta);
		double alpha = 0.25 * (0.5 + beta - gamma);

		double b0 = 2 * alpha;
		double b1 = 2 * 2 * alpha;
		double b2 = 2 * alpha;
		double a1 = 2 * -gamma;
		double a2 = 2 * beta;

		setNormalizedCoefficients(b0, b1, b2, 1, a1, a2);
	} else {
		// When cutoff is zero, nothing gets through the filter, so set
		// coefficients up correctly.
		setNormalizedCoefficients(0, 0, 0, 1, 0, 0);
	}
}

void Biquad::reset()
{
//#if defined( CINDER_AUDIO_VDSP )
//	// Two extra samples for filter history
//	double *in = mInputBuffer.data();
//	in[0] = 0;
//	in[1] = 0;
//
//	double *out = mOutputBuffer.data();
//	out[0] = 0;
//	out[1] = 0;
//
//#else
	m_x1 = m_x2 = m_y1 = m_y2 = 0;
//#endif
}



void Biquad::setNormalizedCoefficients( double b0, double b1, double b2, double a0, double a1, double a2 )
{
	double a0Inverse = 1 / a0;

	m_b0 = b0 * a0Inverse;
	m_b1 = b1 * a0Inverse;
	m_b2 = b2 * a0Inverse;
	m_a1 = a1 * a0Inverse;
	m_a2 = a2 * a0Inverse;
}


} } // namespace cinder::audio2