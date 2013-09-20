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

#include "audio2/NodeTap.h"
#include "audio2/RingBuffer.h"
#include "audio2/Fft.h"

#include "audio2/Debug.h"

#include "cinder/CinderMath.h"

using namespace std;
using namespace ci;

namespace cinder { namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeTap
// ----------------------------------------------------------------------------------------------------

NodeTap::NodeTap( const Format &format )
: Node( format ), mWindowSize( format.getWindowSize() )
{
	if( boost::indeterminate( format.getAutoEnable() ) )
		setAutoEnabled();
}

NodeTap::~NodeTap()
{
}

void NodeTap::initialize()
{
	mCopiedBuffer = Buffer( mWindowSize, getNumChannels() );
	for( size_t ch = 0; ch < getNumChannels(); ch++ )
		mRingBuffers.push_back( unique_ptr<RingBuffer>( new RingBuffer( mWindowSize ) ) );

	mInitialized = true;
}

void NodeTap::process( Buffer *buffer )
{
	for( size_t ch = 0; ch < getNumChannels(); ch++ )
		mRingBuffers[ch]->write( buffer->getChannel( ch ), buffer->getNumFrames() );
}

const Buffer& NodeTap::getBuffer()
{
	for( size_t ch = 0; ch < getNumChannels(); ch++ )
		mRingBuffers[ch]->read( mCopiedBuffer.getChannel( ch ), mCopiedBuffer.getNumFrames() );

	return mCopiedBuffer;
}

const float *NodeTap::getChannel( size_t channel )
{
	CI_ASSERT( channel < mCopiedBuffer.getNumChannels() );

	float *buf = mCopiedBuffer.getChannel( channel );
	mRingBuffers[channel]->read( buf, mCopiedBuffer.getNumFrames() );

	return buf;
}

float NodeTap::getVolume()
{
	const Buffer& buffer = getBuffer();
	return rms( buffer.getData(), buffer.getSize() );
}

float NodeTap::getVolume( size_t channel )
{
	return rms( getChannel( channel ), mCopiedBuffer.getNumFrames() );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeTapSpectral
// ----------------------------------------------------------------------------------------------------

NodeTapSpectral::NodeTapSpectral( const Format &format )
: Node( format ), mFftSize( format.getFftSize() ), mWindowSize( format.getWindowSize() ),
	mWindowType( format.getWindowType() ), mNumFramesCopied( 0 ), mApplyWindow( true ), mSmoothingFactor( 0.65f )
{
}

NodeTapSpectral::~NodeTapSpectral()
{
}

void NodeTapSpectral::initialize()
{
	if( ! mFftSize )
		mFftSize = getContext()->getFramesPerBlock();
	if( ! isPowerOf2( mFftSize ) )
		mFftSize = nextPowerOf2( static_cast<uint32_t>( mFftSize ) );
	
	mFft = unique_ptr<Fft>( new Fft( mFftSize ) );
	mBuffer = audio2::Buffer( mFftSize );
	mBufferSpectral = audio2::BufferSpectral( mFftSize );
	mMagSpectrum.resize( mFftSize / 2 );

	if( ! mWindowSize  )
		mWindowSize = mFftSize;
	else if( ! isPowerOf2( mWindowSize ) )
		mWindowSize = nextPowerOf2( static_cast<uint32_t>( mWindowSize ) );

	mWindow = makeAlignedArray<float>( mWindowSize );
	generateWindow( mWindowType, mWindow.get(), mWindowSize );

	LOG_V << "complete. fft size: " << mFftSize << ", window size: " << mWindowSize << endl;
}

// TODO: should really be using a Converter to go stereo (or more) -> mono
// - a good implementation will use equal-power scaling as if the mono signal was two stereo channels panned to center
void NodeTapSpectral::process( audio2::Buffer *buffer )
{
	lock_guard<mutex> lock( mMutex );

	if( mNumFramesCopied >= mWindowSize )
		return;

	size_t numCopyFrames = std::min( buffer->getNumFrames(), mWindowSize - mNumFramesCopied );
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

const std::vector<float>& NodeTapSpectral::getMagSpectrum()
{
	lock_guard<mutex> lock( mMutex );
	if( mNumFramesCopied == mWindowSize ) {

		if( mApplyWindow )
			multiply( mBuffer.getData(), mWindow.get(), mBuffer.getData(), mWindowSize );

		mFft->forward( &mBuffer, &mBufferSpectral );

		float *real = mBufferSpectral.getReal();
		float *imag = mBufferSpectral.getImag();

		// remove nyquist component.
		imag[0] = 0.0f;

		// compute normalized magnitude spectrum
		const float kMagScale = 1.0f / mFft->getSize();
		for( size_t i = 0; i < mMagSpectrum.size(); i++ ) {
			float re = real[i];
			float im = imag[i];
			mMagSpectrum[i] = mMagSpectrum[i] * mSmoothingFactor + sqrt( re * re + im * im ) * kMagScale * ( 1.0f - mSmoothingFactor );
		}
		mNumFramesCopied = 0;
		mBuffer.zero();
	}
	return mMagSpectrum;
}

void NodeTapSpectral::setSmoothingFactor( float factor )
{
	mSmoothingFactor = ( factor < 0.0f ) ? 0.0f : ( ( factor > 1.0f ) ? 1.0f : factor );
}

} } // namespace cinder::audio2