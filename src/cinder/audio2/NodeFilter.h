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

#include "cinder/audio2/NodeEffect.h"
#include "cinder/audio2/dsp/Biquad.h"

#include <vector>

// TODO: add api for setting biquad with arbitrary set of coefficients, ala pd's [biquad~]

namespace cinder { namespace audio2 {

typedef std::shared_ptr<class NodeFilterLowPass>		NodeFilterLowPassRef;
typedef std::shared_ptr<class NodeFilterHighPass>		NodeFilterHighPassRef;
typedef std::shared_ptr<class NodeFilterBandPass>		NodeFilterBandPassRef;

//! Base class for filter nodes that use Biquad
class NodeFilterBiquad : public NodeEffect {
  public:
	enum Mode { LOWPASS, HIGHPASS, BANDPASS, LOWSHELF, HIGHSHELF, PEAKING, ALLPASS, NOTCH, CUSTOM };

	NodeFilterBiquad( Mode mode = LOWPASS, const Format &format = Format() ) : NodeEffect( format ), mMode( mode ), mCoeffsDirty( true ), mFreq( 200.0f ), mQ( 1.0f ), mGain( 0.0f ) {}
	virtual ~NodeFilterBiquad() {}

	std::string virtual getTag() override			{ return "NodeFilterBiquad"; }

	void initialize() override;
	void uninitialize() override;
	void process( Buffer *buffer ) override;

	void setMode( Mode mode )	{ mMode = mode; mCoeffsDirty = true; }
	Mode getMode() const		{ return mMode; }

	void setFreq( float freq )	{ mFreq = freq; mCoeffsDirty = true; }
	float getFreq() const		{ return mFreq; }

	void setQ( float q )		{ mQ = q; mCoeffsDirty = true; }
	float getQ() const			{ return mQ; }

	void setGain( float gain )	{ mGain = gain; mCoeffsDirty = true; }
	float getGain() const		{ return mGain; }

  protected:
	void updateBiquadParams();

	std::vector<dsp::Biquad> mBiquads;
	std::atomic<bool> mCoeffsDirty;
	BufferT<double> mBufferd;
	size_t mNiquist;

	Mode mMode;
	float mFreq, mQ, mGain;
};

class NodeFilterLowPass : public NodeFilterBiquad {
  public:
	NodeFilterLowPass( const Format &format = Format() ) : NodeFilterBiquad( LOWPASS, format ) {}
	virtual ~NodeFilterLowPass() {}

	std::string virtual getTag() override			{ return "NodeFilterLowPass"; }

	void setCutoffFreq( float freq )			{ setFreq( freq ); }
	void setResonance( float resonance )		{ setQ( resonance ); }

	float getCutoffFreq() const	{ return mFreq; }
	float getResonance() const	{ return mQ; }
};

class NodeFilterHighPass : public NodeFilterBiquad {
public:
	NodeFilterHighPass( const Format &format = Format() ) : NodeFilterBiquad( HIGHPASS, format ) {}
	virtual ~NodeFilterHighPass() {}

	std::string virtual getTag() override			{ return "NodeFilterHighPass"; }

	void setCutoffFreq( float freq )			{ setFreq( freq ); }
	void setResonance( float resonance )		{ setQ( resonance ); }

	float getCutoffFreq() const	{ return mFreq; }
	float getResonance() const	{ return mQ; }
};

class NodeFilterBandPass : public NodeFilterBiquad {
public:
	NodeFilterBandPass( const Format &format = Format() ) : NodeFilterBiquad( BANDPASS, format ) {}
	virtual ~NodeFilterBandPass() {}

	std::string virtual getTag() override			{ return "NodeFilterBandPass"; }

	void setCutoffFreq( float freq )	{ setFreq( freq ); }
	void setWidth( float width )		{ setQ( width ); }

	float getCutoffFreq() const		{ return mFreq; }
	float getWidth() const			{ return mQ; }
};


} } // namespace cinder::audio2