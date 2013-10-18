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

#include "NodeEffect.h"
#include "Biquad.h"

#include <vector>

namespace cinder { namespace audio2 {

typedef std::shared_ptr<class NodeFilterLowPass> NodeFilterLowPassRef;

//! Base class for filter nodes based on the biquad filter
class NodeFilterBiquad : public NodeEffect {
  public:
	NodeFilterBiquad( const Format &format = Format() ) : NodeEffect( format ), mCoeffsDirty( true ) {}
	virtual ~NodeFilterBiquad() {}

	void initialize() override;
	void uninitialize() override;
	void process( Buffer *buffer ) override;

  protected:
	virtual void updateBiquadParams() = 0;

	std::vector<Biquad> mBiquads;
	std::atomic<bool> mCoeffsDirty;
	BufferT<double> mBufferd;
	size_t mNiquist;
};

class NodeFilterLowPass : public NodeFilterBiquad {
  public:
	NodeFilterLowPass( const Format &format = Format() ) : NodeFilterBiquad( format ), mCutoffFreq( 200.0f ), mResonance( 1.0f ) {}
	virtual ~NodeFilterLowPass() {}

	void setCutoffFreq( float freq )			{ mCutoffFreq = freq; mCoeffsDirty = true; }
	void setResonance( float resonance )		{ mResonance = resonance; mCoeffsDirty = true; }

	float getCutoffFreq() const	{ return mCutoffFreq; }
	float getResonance() const	{ return mResonance; }

  private:
	void updateBiquadParams() override;

	float mCutoffFreq, mResonance;
};

class NodeFilterHighPass {

};

} } // namespace cinder::audio2