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

#include "audio2/TapNode.h"
#include "audio2/RingBuffer.h"
#include "audio2/Fft.h"

#include "audio2/Debug.h"

#include "cinder/CinderMath.h"

#include <complex>

using namespace std;
using namespace ci;

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - TapNode
// ----------------------------------------------------------------------------------------------------

TapNode::TapNode( size_t windowSize, const Format &format )
: Node( format ), mWindowSize( windowSize )
{
	mTag = "BufferTap";
	setAutoEnabled();
}

TapNode::~TapNode()
{
}

void TapNode::initialize()
{
	mCopiedBuffer = Buffer( getNumChannels(), mWindowSize );
	for( size_t ch = 0; ch < getNumChannels(); ch++ )
		mRingBuffers.push_back( unique_ptr<RingBuffer>( new RingBuffer( mWindowSize ) ) );

	mInitialized = true;
}

void TapNode::process( Buffer *buffer )
{
	for( size_t ch = 0; ch < getNumChannels(); ch++ )
		mRingBuffers[ch]->write( buffer->getChannel( ch ), buffer->getNumFrames() );
}

const Buffer& TapNode::getBuffer()
{
	for( size_t ch = 0; ch < getNumChannels(); ch++ )
		mRingBuffers[ch]->read( mCopiedBuffer.getChannel( ch ), mCopiedBuffer.getNumFrames() );

	return mCopiedBuffer;
}

const float *TapNode::getChannel( size_t channel )
{
	CI_ASSERT( channel < mCopiedBuffer.getNumChannels() );

	float *buf = mCopiedBuffer.getChannel( channel );
	mRingBuffers[channel]->read( buf, mCopiedBuffer.getNumFrames() );

	return buf;
}

float TapNode::getVolume()
{
	const Buffer& buffer = getBuffer();
	return rms( buffer.getData(), buffer.getSize() );
}

float TapNode::getVolume( size_t channel )
{
	return rms( getChannel( channel ), mCopiedBuffer.getNumFrames() );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - SpectrumTapNode
// ----------------------------------------------------------------------------------------------------

SpectrumTapNode::SpectrumTapNode( size_t fftSize, size_t windowSize, WindowType windowType, const Format &format )
: Node( format ), mFftSize( fftSize ), mWindowSize( windowSize ), mWindowType( windowType ),
	mNumFramesCopied( 0 ), mApplyWindow( true ), mSmoothingFactor( 0.65f )
{
	mTag = "SpectrumTapNode";
	setAutoEnabled();
}

SpectrumTapNode::~SpectrumTapNode()
{
}

void SpectrumTapNode::initialize()
{
	if( ! mFftSize )
		mFftSize = getContext()->getNumFramesPerBlock();
	if( ! isPowerOf2( mFftSize ) )
		mFftSize = nextPowerOf2( mFftSize );
		

	mFft = unique_ptr<Fft>( new Fft( mFftSize ) );

	mBuffer = audio2::Buffer( 1, mFftSize );
	mMagSpectrum.resize( mFftSize / 2 );

	if( ! mWindowSize  )
		mWindowSize = mFftSize;
	else if( ! isPowerOf2( mWindowSize ) )
		mWindowSize = nextPowerOf2( mWindowSize );

	mWindow = makeAlignedArray<float>( mWindowSize );

	switch ( mWindowType ) {
		case WindowType::BLACKMAN:
			generateBlackmanWindow( mWindow.get(), mWindowSize );
			break;
		case WindowType::HAMM:
			generateHammWindow( mWindow.get(), mWindowSize );
			break;
		case WindowType::HANN:
			generateHannWindow( mWindow.get(), mWindowSize );
			break;
		default:
			// rect window, just fill with 1's
			for( size_t i = 0; i < mWindowSize; i++ )
				mWindow.get()[i] = 1.0f;
			break;
	}

	LOG_V << "complete. fft size: " << mFftSize << ", window size: " << mWindowSize << endl;
}

// TODO: should really be using a Converter to go stereo (or more) -> mono
// - a good implementation will use equal-power scaling as if the mono signal was two stereo channels panned to center
void SpectrumTapNode::process( audio2::Buffer *buffer )
{
	lock_guard<mutex> lock( mMutex );

	if( mNumFramesCopied == mWindowSize )
		return;

	size_t numCopyFrames = std::min( buffer->getNumFrames(), mWindowSize - mNumFramesCopied ); // TODO: return if zero
	size_t numSourceChannels = buffer->getNumChannels();
	float *offsetBuffer = &mBuffer[mNumFramesCopied];
	if( numSourceChannels == 1 ) {
		memcpy( offsetBuffer, buffer->getData(), numCopyFrames * sizeof( float ) );
	}
	else {
		// naive average of all channels
		float scale = 1.0f / numSourceChannels;
		for( size_t ch = 0; ch < numSourceChannels; ch++ ) {
			for( size_t i = 0; i < numCopyFrames; i++ )
				offsetBuffer[i] += buffer->getChannel( ch )[i] * scale;
		}
	}

	mNumFramesCopied += numCopyFrames;
}

const std::vector<float>& SpectrumTapNode::getMagSpectrum()
{
	lock_guard<mutex> lock( mMutex );
	if( mNumFramesCopied == mWindowSize ) {

		if( mApplyWindow ) {
			float *win = mWindow.get();
			for( size_t i = 0; i < mWindowSize; ++i )
				mBuffer[i] *= win[i];
		}

		mFft->compute( &mBuffer );

		auto &real = mFft->getReal();
		auto &imag = mFft->getImag();

		// remove nyquist component.
		imag[0] = 0.0f;

		// compute normalized magnitude spectrum
		const float kMagScale = 1.0f / mFft->getSize();
		for( size_t i = 0; i < mMagSpectrum.size(); i++ ) {
			complex<float> c( real[i], imag[i] );
			mMagSpectrum[i] = mMagSpectrum[i] * mSmoothingFactor + abs( c ) * kMagScale * ( 1.0f - mSmoothingFactor );
		}
		mNumFramesCopied = 0;
		mBuffer.zero();
	}
	return mMagSpectrum;
}

void SpectrumTapNode::setSmoothingFactor( float factor )
{
	mSmoothingFactor = ( factor < 0.0f ) ? 0.0f : ( ( factor > 1.0f ) ? 1.0f : factor );
}

} // namespace audio2