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

#pragma once

#include "audio2/Context.h"
#include "audio2/Dsp.h"

namespace audio2 {

class RingBuffer;
class Fft;

typedef std::shared_ptr<class TapNode> TapNodeRef;
typedef std::shared_ptr<class SpectrumTapNode> SpectrumTapNodeRef;

class TapNode : public Node {
public:
	TapNode( size_t numBufferedFrames = 1024, const Format &format = Format() );
	virtual ~TapNode();

	const float* getChannel( size_t ch = 0 );
	const Buffer& getBuffer();

	virtual void initialize() override;
	virtual void process( Buffer *buffer ) override;

private:
	std::vector<std::unique_ptr<RingBuffer> > mRingBuffers; // TODO: make this one continuous buffer so it better matches audio::Buffer
	Buffer mCopiedBuffer;
	size_t mNumBufferedFrames;
};

class SpectrumTapNode : public Node {
public:
	//! If fftSize is not set, defaults to Context::getNumFramesPerBlock(). If window size is not set, defaults to fftSize
	SpectrumTapNode( size_t fftSize = 0, size_t windowSize = 0, WindowType windowType = WindowType::BLACKMAN, const Format &format = Format() );
	virtual ~SpectrumTapNode();
	
	virtual void initialize() override;
	virtual void process( audio2::Buffer *buffer ) override;

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
	audio2::Buffer mBuffer;
	std::vector<float> mMagSpectrum;
	AlignedArrayPtr mWindow;
	std::atomic<bool> mApplyWindow;
	std::atomic<size_t> mNumFramesCopied;
	size_t mWindowSize, mFftSize;
	WindowType mWindowType;
	float mSmoothingFactor;
};

} // namespace audio2