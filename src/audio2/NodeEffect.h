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
#include "cinder/CinderMath.h"

namespace cinder { namespace audio2 {

typedef std::shared_ptr<class NodeEffect>		NodeEffectRef;
typedef std::shared_ptr<class NodeGain>			NodeGainRef;
typedef std::shared_ptr<class NodePan2d>		NodePan2dRef;

class NodeEffect : public Node {
  public:
	NodeEffect( const Format &format = Format() );
	virtual ~NodeEffect() {}
};

class NodeGain : public NodeEffect {
  public:
	NodeGain( const Format &format = Format() ) : NodeEffect( format ), mGain( 1.0f ), mMin( 0.0f ), mMax( 1.0f ) {}
	virtual ~NodeGain() {}

	std::string virtual getTag() override			{ return "NodeGain"; }

	void process( Buffer *buffer ) override;

	void setGain( float linear )	{ mGain = ci::math<float>::clamp( linear, mMin, mMax ); }
	float getGain() const			{ return mGain; }

	void setMin( float min )		{ mMin = min; }
	float getMin() const			{ return mMin; }
	void setMax( float max )		{ mMax = max; }
	float getMax() const			{ return mMax; }

  private:
	std::atomic<float> mGain, mMin, mMax;
};

//! Simple stereo panning using an equal power cross-fade.
class NodePan2d : public NodeEffect {
  public:
	NodePan2d( const Format &format = Format() );
	virtual ~NodePan2d() {}

	std::string virtual getTag() override			{ return "NodePan2d"; }

	void process( Buffer *buffer ) override;

	//! Sets the panning position in range of [0:1]: 0 = left, 1 = right, and 0.5 = center.
	void setPos( float pos );
	//! Gets the current
	float getPos() const	{ return mPos; }

  private:
	std::atomic<float> mPos;
};

class RingMod : public NodeEffect {
  public:
	RingMod( const Format &format = Format() ) : NodeEffect( format ), mSineGen( 440.0f, 1.0f )	{}
	virtual ~RingMod() {}

	std::string virtual getTag() override			{ return "RingMod"; }

	virtual void initialize() override {
		mSineGen.setSampleRate( getContext()->getSampleRate() );
	}

	virtual void process( Buffer *buffer );

	SineGen mSineGen;
	std::vector<float> mSineBuffer;
};

} } // namespace cinder::audio2