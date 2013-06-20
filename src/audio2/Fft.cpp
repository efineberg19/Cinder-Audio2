#include "audio2/Fft.h"
#include "audio2/Dsp.h"
#include "audio2/Assert.h"

namespace audio2 {

	Fft::Fft( size_t fftSize )
	{
		mSize = forcePow2( fftSize );
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