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

#include "cinder/audio2/Scope.h"
#include "cinder/audio2/dsp/RingBuffer.h"
#include "cinder/audio2/dsp/Fft.h"
#include "cinder/audio2/Utilities.h"
#include "cinder/audio2/Debug.h"

#include "cinder/CinderMath.h"

using namespace std;
using namespace ci;

namespace cinder { namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - Scope
// ----------------------------------------------------------------------------------------------------

Scope::Scope( const Format &format )
	: NodeAutoPullable( format ), mWindowSize( format.getWindowSize() ), mRingBufferPaddingFactor( 2 )
{
	if( boost::indeterminate( format.getAutoEnable() ) )
		setAutoEnabled();
}

Scope::~Scope()
{
}

void Scope::initialize()
{
	if( ! mWindowSize )
		mWindowSize = getFramesPerBlock();
	else if( ! isPowerOf2( mWindowSize ) )
		mWindowSize = nextPowerOf2( static_cast<uint32_t>( mWindowSize ) );

	for( size_t ch = 0; ch < mNumChannels; ch++ )
		mRingBuffers.emplace_back( mWindowSize * mRingBufferPaddingFactor );

	mCopiedBuffer = Buffer( mWindowSize, getNumChannels() );
}

void Scope::process( Buffer *buffer )
{
	size_t numFrames = min( buffer->getNumFrames(), mRingBuffers[0].getSize() );
	for( size_t ch = 0; ch < mNumChannels; ch++ ) {
		if( ! mRingBuffers[ch].write( buffer->getChannel( ch ), numFrames ) )
			return;
	}
}

const Buffer& Scope::getBuffer()
{
	fillCopiedBuffer();
	return mCopiedBuffer;
}

float Scope::getVolume()
{
	fillCopiedBuffer();
	return dsp::rms( mCopiedBuffer.getData(), mCopiedBuffer.getSize() );
}

float Scope::getVolume( size_t channel )
{
	fillCopiedBuffer();
	return dsp::rms( mCopiedBuffer.getChannel( channel ), mCopiedBuffer.getNumFrames() );
}

void Scope::fillCopiedBuffer()
{
	for( size_t ch = 0; ch < mNumChannels; ch++ ) {
		if( ! mRingBuffers[ch].read( mCopiedBuffer.getChannel( ch ), mCopiedBuffer.getNumFrames() ) )
			return;
	}
}

// ----------------------------------------------------------------------------------------------------
// MARK: - ScopeSpectral
// ----------------------------------------------------------------------------------------------------

ScopeSpectral::ScopeSpectral( const Format &format )
	: Scope( format ), mFftSize( format.getFftSize() ), mWindowType( format.getWindowType() ), mSmoothingFactor( 0.5f )
{
}

ScopeSpectral::~ScopeSpectral()
{
}

void ScopeSpectral::initialize()
{
	Scope::initialize();

	if( mFftSize < mWindowSize )
		mFftSize = mWindowSize;
	if( ! isPowerOf2( mFftSize ) )
		mFftSize = nextPowerOf2( static_cast<uint32_t>( mFftSize ) );
	
	mFft = unique_ptr<dsp::Fft>( new dsp::Fft( mFftSize ) );
	mFftBuffer = audio2::Buffer( mFftSize );
	mBufferSpectral = audio2::BufferSpectral( mFftSize );
	mMagSpectrum.resize( mFftSize / 2 );

	if( ! mWindowSize  )
		mWindowSize = mFftSize;
	else if( ! isPowerOf2( mWindowSize ) )
		mWindowSize = nextPowerOf2( static_cast<uint32_t>( mWindowSize ) );

	mWindowingTable = makeAlignedArray<float>( mWindowSize );
	generateWindow( mWindowType, mWindowingTable.get(), mWindowSize );
}

// TODO: When mNumChannels > 1, use generic channel converter.
// - alternatively, this tap can force mono output, which only works if it isn't a tap but is really a leaf node (no output).
const std::vector<float>& ScopeSpectral::getMagSpectrum()
{
	fillCopiedBuffer();

	// window the copied buffer and compute forward FFT transform
	if( mNumChannels > 1 ) {
		// naive average of all channels
		mFftBuffer.zero();
		float scale = 1.0f / mNumChannels;
		for( size_t ch = 0; ch < mNumChannels; ch++ ) {
			for( size_t i = 0; i < mWindowSize; i++ )
				mFftBuffer[i] += mCopiedBuffer.getChannel( ch )[i] * scale;
		}
		dsp::mul( mFftBuffer.getData(), mWindowingTable.get(), mFftBuffer.getData(), mWindowSize );
	}
	else
		dsp::mul( mCopiedBuffer.getData(), mWindowingTable.get(), mFftBuffer.getData(), mWindowSize );

	mFft->forward( &mFftBuffer, &mBufferSpectral );

	float *real = mBufferSpectral.getReal();
	float *imag = mBufferSpectral.getImag();

	// remove nyquist component
	imag[0] = 0.0f;

	// compute normalized magnitude spectrum
	// TODO: break this into vector cartisian -> polar and then vector lowpass. skip lowpass if smoothing factor is very small
	const float magScale = 1.0f / mFft->getSize();
	for( size_t i = 0; i < mMagSpectrum.size(); i++ ) {
		float re = real[i];
		float im = imag[i];
		mMagSpectrum[i] = mMagSpectrum[i] * mSmoothingFactor + sqrt( re * re + im * im ) * magScale * ( 1 - mSmoothingFactor );
	}

	return mMagSpectrum;
}

void ScopeSpectral::setSmoothingFactor( float factor )
{
	mSmoothingFactor = math<float>::clamp( factor );
}

float ScopeSpectral::getFreqForBin( size_t bin )
{
	return bin * getSampleRate() / (float)getFftSize();
}

} } // namespace cinder::audio2