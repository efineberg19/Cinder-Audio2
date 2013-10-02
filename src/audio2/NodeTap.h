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

// TODO: consider NodeTap as a base class
//		- all subclasses auto-enabled
//		- examples: WaveformTapNode, VolumeTapNode, NodeTapSpectral, OnsetTapNode, PitchTapNode

#pragma once

#include "audio2/Context.h"
#include "audio2/Dsp.h"
#include "audio2/RingBuffer.h"

#include "cinder/Thread.h"

namespace cinder { namespace audio2 {

class Fft;

typedef std::shared_ptr<class NodeTap> NodeTapRef;
typedef std::shared_ptr<class NodeTapSpectral> NodeTapSpectralRef;

class NodeTapBase : public Node {

};

class NodeTap : public Node {
  public:
	struct Format : public Node::Format {
		Format() : mWindowSize( 0 ) {}

		//! Sets the window size, the number of samples that are recorded for one 'window' into the audio signal. Default is the Context's frames-per-block.
		Format& windowSize( size_t size )		{ mWindowSize = size; return *this; }
		//! Returns the window size.
		size_t getWindowSize() const			{ return mWindowSize; }

	  protected:
		size_t mWindowSize;
	};

	NodeTap( const Format &format = Format() );
	virtual ~NodeTap();

	std::string virtual getTag() override			{ return "NodeTap"; }

	const Buffer& getBuffer();

	//! Compute the average (RMS) volume across all channels
	float getVolume();
	//! Compute the average (RMS) volume across \a channel
	float getVolume( size_t channel );

	virtual void initialize() override;
	virtual void process( Buffer *buffer ) override;

  protected:
	//! Copies audio frames from the RingBuffer into mCopiedBuffer, which is suitable for operation on the main thread.
	void fillCopiedBuffer();
	
	RingBuffer	mRingBuffer;
	Buffer		mCopiedBuffer;
	size_t		mWindowSize;
	size_t		mRingBufferPaddingFactor;
};

class NodeTapSpectral : public NodeTap {
  public:
	struct Format : public NodeTap::Format {
		Format() : NodeTap::Format(), mFftSize( 0 ), mWindowType( WindowType::BLACKMAN ) {}

		//! Sets the FFT size, rounded up to the nearest power of 2 greated than \a windowSize. Setting this larger than \a windowSize causes the FFT transform to be 'zero-padded'. Default is the same as windowSize.
		Format&		fftSize( size_t size )			{ mFftSize = size; return *this; }
		//! defaults to WindowType::BLACKMAN
		Format&		windowType( WindowType type )	{ mWindowType = type; return *this; }
		//! \see NodeTap::windowSize()
		Format&		windowSize( size_t size )		{ NodeTap::Format::windowSize( size ); return *this; }

		size_t		getFftSize() const				{ return mFftSize; }
		WindowType	getWindowType() const			{ return mWindowType; }

      protected:
		size_t		mFftSize;
		WindowType	mWindowType;
	};

	NodeTapSpectral( const Format &format = Format() );
	virtual ~NodeTapSpectral();

	std::string virtual getTag() override			{ return "NodeTapSpectral"; }

	virtual void initialize() override;

	const std::vector<float>& getMagSpectrum();

	size_t getFftSize() const	{ return mFftSize; }

	float getSmoothingFactor() const	{ return mSmoothingFactor; }
	void setSmoothingFactor( float factor );

  private:
	std::unique_ptr<Fft>	mFft;
	Buffer					mFftBuffer;			// windowed samples before transform
	BufferSpectral			mBufferSpectral;	// transformed samples
	std::vector<float>		mMagSpectrum;		// computed magnitude spectrum from frequency-domain samples
	AlignedArrayPtr			mWindowingTable;
//	std::atomic<size_t>		mNumFramesCopied;
	size_t					mFftSize;
	WindowType				mWindowType;
	float					mSmoothingFactor;
};

} } // namespace cinder::audio2