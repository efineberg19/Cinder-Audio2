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

#include "audio2/Dsp.h"

#include "cinder/CinderMath.h"

using namespace ci;

namespace audio2 {

#if defined( CINDER_AUDIO_DSP_ACCELERATE )

void generateBlackmanWindow( float *window, size_t length )
{
	vDSP_blkman_window( window, static_cast<vDSP_Length>( length ), 0 );
}

void generateHammWindow( float *window, size_t length )
{
	vDSP_hamm_window( window, static_cast<vDSP_Length>( length ), 0 );
}

void generateHannWindow( float *window, size_t length )
{
	vDSP_hann_window( window, static_cast<vDSP_Length>( length ), 0 );
}

void fill( float value, float *audioData, size_t length )
{
	vDSP_vfill( audioData, audioData, value, length );
}

float rms( const float *audioData, size_t length )
{
	float result;

	// note: OS X 10.8 specifies float pointer as first parm, while 10.9 is const float pointer
	vDSP_rmsqv( const_cast<float *>( audioData ), 1, &result, length );
	return result;
}

#else

// from WebKit's applyWindow in RealtimeAnalyser.cpp
void generateBlackmanWindow( float *window, size_t length )
{
	double alpha = 0.16;
	double a0 = 0.5 * (1 - alpha);
	double a1 = 0.5;
	double a2 = 0.5 * alpha;
	double oneOverN = 1.0 / static_cast<double>( length );

	for( size_t i = 0; i < length; ++i ) {
		double x = static_cast<double>(i) * oneOverN;
		window[i] = float( a0 - a1 * cos( 2.0 * M_PI * x ) + a2 * cos( 4.0 * M_PI * x ) );
	}
}

void generateHammWindow( float *window, size_t length )
{
	CI_ASSERT( 0 && "not implemented" );
}

void generateHannWindow( float *window, size_t length )
{
	CI_ASSERT( 0 && "not implemented" );
}

void fill( float value, float *audioData, size_t length )
{
	for( size_t i = 0; i < length; i++ )
		audioData[i] = value;
}

float rms( const float *audioData, size_t length )
{
	float sumSquared = 0;
	for( size_t i = 0; i < length; i++ ) {
		float val = audioData[i];
		sumSquared += val * val;
	}

	return math<float>::sqrt( sumSquared / (float)length );
}

#endif


void generateWindow( WindowType windowType, float *window, size_t length )
{
	switch ( windowType ) {
		case WindowType::BLACKMAN:
			generateBlackmanWindow( window, length );
			break;
		case WindowType::HAMM:
			generateHammWindow( window, length );
			break;
		case WindowType::HANN:
			generateHannWindow( window, length );
			break;
		case WindowType::RECT:
		default:
			fill( 1.0f, window, length );
			break;
	}
}


} // namespace audio2