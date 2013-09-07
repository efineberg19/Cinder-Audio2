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

// TODO: consider TapNode as a base class
//		- all subclasses auto-enabled
//		- examples: WaveformTapNode, VolumeTapNode, SpectrumTapNode, OnsetTapNode, PitchTapNode

#pragma once

#include "audio2/Context.h"
#include "audio2/Dsp.h"

#include "cinder/Thread.h"

namespace cinder { namespace audio2 {

class RingBuffer;
class Fft;

typedef std::shared_ptr<class TapNode> TapNodeRef;
typedef std::shared_ptr<class SpectrumTapNode> SpectrumTapNodeRef;

class TapNode : public Node {
public:
	struct Format : public Node::Format {
		Format() : mWindowSize( 1024 ) {}

		Format& windowSize( size_t size )		{ mWindowSize = size; return *this; }
		size_t getWindowSize() const			{ return mWindowSize; }

	protected:
		size_t mWindowSize;
	};

	TapNode( const Format &format = Format() );
	virtual ~TapNode();

	std::string virtual getTag() override			{ return "TapNode"; }

	const float* getChannel( size_t ch = 0 );
	const Buffer& getBuffer();

	//! Compute the average (RMS) volume across all channels
	float getVolume();
	//! Compute the averate (RMS) volume across \a channel
	float getVolume( size_t channel );

	virtual void initialize() override;
	virtual void process( Buffer *buffer ) override;

private:
	std::vector<std::unique_ptr<RingBuffer> > mRingBuffers;
	Buffer mCopiedBuffer;
	size_t mWindowSize;
};

class SpectrumTapNode : public Node {
public:
	struct Format : public Node::Format {
		Format() : mFftSize( 0 ), mWindowSize( 0 ), mWindowType( WindowType::BLACKMAN ) {}

		//! defaults to Context's frames-per-block
		Format&		fftSize( size_t size )			{ mFftSize = size; return *this; }
		size_t		getFftSize() const				{ return mFftSize; }

		//! If window size is not set, defaults to fftSize. If fftSize is not set, defaults to Context::getFramesPerBlock().
		Format&		windowSize( size_t size )		{ mWindowSize = size; return *this; }
		size_t		getWindowSize() const			{ return mWindowSize; }

		//! defaults to WindowType::BLACKMAN
		Format&		windowType( WindowType type )	{ mWindowType = type; return *this; }
		WindowType	getWindowType() const			{ return mWindowType; }

	protected:
		size_t mWindowSize, mFftSize;
		WindowType mWindowType;
	};

	SpectrumTapNode( const Format &format = Format() );
	virtual ~SpectrumTapNode();

	std::string virtual getTag() override			{ return "SpectrumTapNode"; }

	virtual void initialize() override;
	virtual void process( Buffer *buffer ) override;

	const std::vector<float>& getMagSpectrum();

	void setWindowingEnabled( bool b = true )	{ mApplyWindow = b; }
	bool isWindowingEnabled() const				{ return mApplyWindow; }

	size_t getFftSize() const	{ return mFftSize; }

	float getSmoothingFactor() const	{ return mSmoothingFactor; }
	void setSmoothingFactor( float factor );

private:
	std::unique_ptr<Fft> mFft;
	std::mutex mMutex;

	// TODO: consider storing this in Fft - it has to be the same size as Fft::getSize
	// - but all 'TapNode's could use this - move it to base class?
	Buffer mBuffer;
	BufferSpectral mBufferSpectral;
	std::vector<float> mMagSpectrum;
	AlignedArrayPtr mWindow;
	std::atomic<bool> mApplyWindow;
	std::atomic<size_t> mNumFramesCopied;
	size_t mWindowSize, mFftSize;
	WindowType mWindowType;
	float mSmoothingFactor;
};

} } // namespace cinder::audio2