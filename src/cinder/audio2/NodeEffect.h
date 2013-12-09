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

#include "cinder/audio2/Context.h"
#include "cinder/audio2/dsp/Dsp.h"
#include "cinder/audio2/Param.h"
#include "cinder/CinderMath.h"

namespace cinder { namespace audio2 {

typedef std::shared_ptr<class NodeEffect>		NodeEffectRef;
typedef std::shared_ptr<class Gain>			GainRef;
typedef std::shared_ptr<class Pan2d>		Pan2dRef;

class NodeEffect : public Node {
  public:
	NodeEffect( const Format &format = Format() );
	virtual ~NodeEffect() {}
};

class Gain : public NodeEffect {
  public:
	Gain( const Format &format = Format() );
	virtual ~Gain() {}

	std::string virtual getTag() override			{ return "Gain"; }

	void process( Buffer *buffer ) override;

	void setValue( float linear )	{ mGain.setValue( ci::math<float>::clamp( linear, mMin, mMax ) ); }
	float getValue() const			{ return mGain.getValue(); }

	Param* getParam()			{ return &mGain; }

	void setMin( float min )		{ mMin = min; }
	float getMin() const			{ return mMin; }
	void setMax( float max )		{ mMax = max; }
	float getMax() const			{ return mMax; }

  private:
	Param				mGain;
	std::atomic<float>	mMin, mMax;
};

//! Simple stereo panning using an equal power cross-fade. The panning position is specified by a single position between the left and right speakers.
class Pan2d : public NodeEffect {
  public:
	//! Constructs a Pan2d. \note Format::channel() and Format::channelMode() are ignored and number of output channels is always 2.
	Pan2d( const Format &format = Format() );
	virtual ~Pan2d() {}

	std::string virtual getTag() override			{ return "Pan2d"; }

//	//! Overridden to handle mono input without upmixing
//	bool supportsInputNumChannels( size_t numChannels ) override;
//	//! Overridden to handle mono input without upmixing
//	void pullInputs( Buffer *destBuffer ) override;

	void process( Buffer *buffer ) override;

	//! Sets the panning position in range of [0:1]: 0 = left, 1 = right, and 0.5 = center.
	void setPos( float pos );
	//! Gets the current
	float getPos() const	{ return mPos; }

	void enableMonoInputMode( bool enable )	{ mMonoInputMode = enable; }
	bool isMonoInputModeEnabled() const		{ return mMonoInputMode; }

  private:
	std::atomic<float>	mPos;
	bool				mMonoInputMode;
};

} } // namespace cinder::audio2