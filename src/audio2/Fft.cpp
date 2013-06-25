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
#include "audio2/Dsp.h"
#include "audio2/Assert.h"
#include "audio2/audio.h"

namespace audio2 {

Fft::Fft( size_t fftSize )
: mSize( fftSize )
{
	if( ! mSize || ! isPowerOf2( mSize ) )
		throw AudioExc( "invalid fftSize" );

	mLog2FftSize = log2f( mSize );

	mReal.resize( mSize );
	mImag.resize( mSize );

#if defined( CINDER_AUDIO_FFT_ACCELERATE )
	mSplitComplexFrame.realp = mReal.data();
	mSplitComplexFrame.imagp = mImag.data();

	mFftSetup = vDSP_create_fftsetup( mLog2FftSize, FFT_RADIX2 );
	CI_ASSERT( mFftSetup );
#endif // defined( CINDER_AUDIO_FFT_ACCELERATE )

}

Fft::~Fft()
{
#if defined( CINDER_AUDIO_FFT_ACCELERATE )
	vDSP_destroy_fftsetup( mFftSetup );
#endif // defined( CINDER_AUDIO_FFT_ACCELERATE )
}


void Fft::compute( Buffer *buffer )
{
	CI_ASSERT( buffer->getNumFrames() == mSize );
	
#if defined( CINDER_AUDIO_FFT_ACCELERATE )

	vDSP_ctoz( ( ::DSPComplex *)buffer->getData(), 2, &mSplitComplexFrame, 1, mSize / 2 );
	vDSP_fft_zrip( mFftSetup, &mSplitComplexFrame, 1, mLog2FftSize, FFT_FORWARD );

#endif // defined( CINDER_AUDIO_FFT_ACCELERATE )
}


} // namespace audio2