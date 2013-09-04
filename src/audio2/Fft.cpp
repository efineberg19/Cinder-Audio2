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

#include "audio2/Fft.h"
#include "audio2/CinderAssert.h"
#include "audio2/audio.h"

#if defined( CINDER_AUDIO_OOURA )
	#include "audio2/ooura/fftsg.h"
#endif

namespace cinder { namespace audio2 {

Fft::Fft( size_t fftSize )
: mSize( fftSize )
{
	if( mSize < 2 || ! isPowerOf2( mSize ) )
		throw AudioExc( "invalid fftSize" );

	mSizeOverTwo = mSize / 2;

	init();
}

#if defined( CINDER_AUDIO_VDSP )

void Fft::init()
{
	mLog2FftSize = log2f( mSize );
	mFftSetup = vDSP_create_fftsetup( mLog2FftSize, FFT_RADIX2 );
	CI_ASSERT( mFftSetup );
}

Fft::~Fft()
{
	vDSP_destroy_fftsetup( mFftSetup );
}

// TODO: use vDSP_fft_zop
void Fft::forward( Buffer *waveform, BufferSpectral *spectral )
{
	CI_ASSERT( waveform->getNumFrames() == mSize );
	CI_ASSERT( spectral->getNumFrames() == mSizeOverTwo );

	mSplitComplexFrame.realp = spectral->getReal();
	mSplitComplexFrame.imagp = spectral->getImag();

	vDSP_ctoz( (::DSPComplex *)waveform->getData(), 2, &mSplitComplexFrame, 1, mSizeOverTwo );
	vDSP_fft_zrip( mFftSetup, &mSplitComplexFrame, 1, mLog2FftSize, FFT_FORWARD );
}

void Fft::inverse( BufferSpectral *spectral, Buffer *waveform )
{
	CI_ASSERT( waveform->getNumFrames() == mSize );
	CI_ASSERT( spectral->getNumFrames() == mSizeOverTwo );

	mSplitComplexFrame.realp = spectral->getReal();
	mSplitComplexFrame.imagp = spectral->getImag();
	float *data = waveform->getData();

	vDSP_fft_zrip( mFftSetup, &mSplitComplexFrame, 1, mLog2FftSize, FFT_INVERSE );
	vDSP_ztoc( &mSplitComplexFrame, 1, (::DSPComplex *)data, 2, mSizeOverTwo );

	float scale = 1.0f / float( 2 * mSize );
	vDSP_vsmul( data, 1, &scale, data, 1, mSize );
}

#elif defined( CINDER_AUDIO_OOURA )

void Fft::init()
{
	mOouraIp = (int *)calloc( 2 + sqrt( mSizeOverTwo ), sizeof( int ) );
	mOouraW = (float *)calloc( mSizeOverTwo, sizeof( float ) );
}

Fft::~Fft()
{
	free( mOouraIp );
	free( mOouraW );
}

void Fft::forward( Buffer *waveform, BufferSpectral *spectral )
{
	CI_ASSERT( waveform->getNumFrames() == mSize );
	CI_ASSERT( spectral->getNumFrames() == mSizeOverTwo );

	float *a = waveform->getData();
	float *real = spectral->getReal();
	float *imag = spectral->getImag();

	// FIXME: don't modify waveform
	ooura::rdft( (int)mSize, 1, a, mOouraIp, mOouraW );

	real[0] = a[0];
	imag[0] = a[1];

	for( size_t k = 1; k < mSizeOverTwo; k++ ) {
		real[k] = a[k * 2];
		imag[k] = a[k * 2 + 1];
	}
}

void Fft::inverse( BufferSpectral *spectral, Buffer *waveform )
{
	CI_ASSERT( waveform->getNumFrames() == mSize );
	CI_ASSERT( spectral->getNumFrames() == mSizeOverTwo );

	float *real = spectral->getReal();
	float *imag = spectral->getImag();
	float *a = waveform->getData();

	a[0] = real[0];
	a[1] = imag[0];

	for( size_t k = 1; k < mSizeOverTwo; k++ ) {
		a[k * 2] = real[k];
		a[k * 2 + 1] = imag[k];
	}

	ooura::rdft( (int)mSize, -1, a, mOouraIp, mOouraW );
	multiply( a, 2.0f / (float)mSize, a, mSize );
}

#endif // defined( CINDER_AUDIO_OOURA )

} } // namespace cinder::audio2