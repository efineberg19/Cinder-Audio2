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

#if defined( CINDER_AUDIO_VDSP )
	#include <Accelerate/Accelerate.h>
#endif

using namespace ci;

namespace cinder { namespace audio2 {

#if defined( CINDER_AUDIO_VDSP )

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

void fill( float value, float *array, size_t length )
{
	vDSP_vfill( array, array, value, length );
}

float sum( const float *array, size_t length )
{
	float result;
	vDSP_svemg( const_cast<float *>( array ), 1, &result, length );
	return result;
}

void add( const float *arrayA, const float *arrayB, float *result, size_t length )
{
	vDSP_vadd( arrayA, 1, arrayB, 1, result, 1, length );
}

float rms( const float *array, size_t length )
{
	float result;
	vDSP_rmsqv( const_cast<float *>( array ), 1, &result, length );
	return result;
}

void multiply( const float *array, float scalar, float *result, size_t length )
{
	vDSP_vsmul( array, 1, &scalar, result, 1, length );
}

void multiply( const float *arrayA, const float *arrayB, float *result, size_t length )
{
	vDSP_vmul( arrayA, 1, arrayB, 1, result, 1, length );
}

#else // ! defined( CINDER_AUDIO_VDSP )

// from WebKit's applyWindow in RealtimeAnalyser.cpp
void generateBlackmanWindow( float *window, size_t length )
{
	double alpha = 0.16;
	double a0 = 0.5 * (1 - alpha);
	double a1 = 0.5;
	double a2 = 0.5 * alpha;
	double oneOverN = 1.0 / static_cast<double>( length );

	for( size_t i = 0; i < length; i++ ) {
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

void fill( float value, float *array, size_t length )
{
	for( size_t i = 0; i < length; i++ )
		array[i] = value;
}

float sum( const float *array, size_t length )
{
	float result( 0.0f );
	for( size_t i = 0; i < length; i++ )
		result += array[i];
	return result;
}

void add( const float *arrayA, const float *arrayB, float *result, size_t length )
{
	for( size_t i = 0; i < length; i++ )
		result[i] = arrayA[i] + arrayB[i];
}

float rms( const float *array, size_t length )
{
	float sumSquared( 0.0f );
	for( size_t i = 0; i < length; i++ ) {
		float val = array[i];
		sumSquared += val * val;
	}

	return math<float>::sqrt( sumSquared / (float)length );
}

void multiply( const float *array, float scalar, float *result, size_t length )
{
	for( size_t i = 0; i < length; i++ )
		result[i] = array[i] * scalar;
}

void multiply( const float *arrayA, const float *arrayB, float *result, size_t length )
{
	for( size_t i = 0; i < length; i++ )
		result[i] = arrayA[i] * arrayB[i];
}

#endif // ! defined( CINDER_AUDIO_VDSP )


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

} } // namespace cinder::audio2