#pragma once

#include "cinder/Cinder.h"

#include "audio2/Buffer.h"

#if defined( CINDER_COCOA )
	#define CINDER_AUDIO_FFT_ACCELERATE
	#include <Accelerate/Accelerate.h>
#else
	#warning "FFT not yet implemented for this platform"
#endif

#include <vector>

namespace audio2 {

class Fft {
public:
	Fft( size_t fftSize = 512 );
	~Fft();

	void compute( Buffer *buffer );

	// TODO: implement
	void computeInverse() {}

	size_t getSize() const	{ return mSize; }

	std::vector<float>& getReal()	{ return mReal; }
	std::vector<float>& getImag()	{ return mImag; }

	const std::vector<float>& getReal() const	{ return mReal; }
	const std::vector<float>& getImag()	const	{ return mImag; }

protected:
	std::vector<float> mReal, mImag;
	size_t mSize;

#if defined( CINDER_AUDIO_FFT_ACCELERATE )
	size_t mLog2FftSize;
	::FFTSetup mFftSetup;
	::DSPSplitComplex mSplitComplexFrame;
#endif
};

} // namespace audio2