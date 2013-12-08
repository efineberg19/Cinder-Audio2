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

#include "cinder/audio2/Buffer.h"

#include <list>
#include <vector>

#include <functional>

namespace cinder { namespace audio2 {

typedef std::shared_ptr<class Context>		ContextRef;
typedef std::shared_ptr<class Node>			NodeRef;

// TODO: add rampLog
// ???: why does webaudio use expo? is it an EaseOutExpo?
// note: unless we want to add _VARIADIC_MAX=6 in preprocessor definitions to all projects, number of args here has to be 5 or less for vc11 support
void rampLinear( float *array, size_t count, float valueBegin, float valueEnd, const std::pair<float, float> &timeRangeNormalized );
void rampExpo( float *array, size_t count, float valueBegin, float valueEnd, const std::pair<float, float> &timeRangeNormalized );

class Param {
  public:
	typedef std::function<void ( float *, size_t, float, float, const std::pair<float, float>& )>	RampFn;

	explicit Param( float initialValue = 0.0f ) : mValue( initialValue ) {}

	void initialize( const ContextRef &context );

	float	getValue() const	{ return mValue; }
	void	setValue( float value );

	struct Options {
		Options() : mDelay( 0.0f ), mRampFn( rampLinear ) {}

		Options& delay( float delay )				{ mDelay = delay; return *this; }
		Options& rampFn( const RampFn &rampFn )		{ mRampFn = rampFn; return *this; }

		float getDelay() const				{ return mDelay; }
		const RampFn&	getRampFn() const	{ return mRampFn; }

	private:
		float mDelay;
		RampFn	mRampFn;
	};

	//! Replaces any existing events with a ramp event from the current value to \a endValue over \a rampSeconds, according to \a options. If there is an existing modulator, it is disconnected.
	void applyRamp( float endValue, float rampSeconds, const Options &options = Options() );
	//! Replaces any existing ramps param manipulations with a ramp event from \a beginValue to \a endValue over \a rampSeconds, according to \a options. If there is an existing modulator, it is disconnected.
	void applyRamp( float beginValue, float endValue, float rampSeconds, const Options &options = Options() );
	//! Appends a ramp event from the end of the last scheduled event (or the current time) to \a endValue over \a rampSeconds, according to \a options. If there is an existing modulator, it is disconnected.
	void appendRamp( float endValue, float rampSeconds, const Options &options = Options() );

	//TODO: make sure ramps behave well with this
	void setModulator( const NodeRef node );

	void reset();
	size_t getNumEvents() const;
	
	bool	isVaryingThisBlock() const;

	float*	getValueArray();
	void	eval( float timeBegin, float *array, size_t arrayLength, size_t sampleRate );

	float					findDuration() const;
	std::pair<float, float> findEndTimeAndValue() const;

  private:
	struct Event {
		Event() : mFramesProcessed( 0 ) {}
		Event( float timeBegin, float timeEnd, float valueBegin, float valueEnd, const RampFn &rampFn );

		float	mTimeBegin, mTimeEnd, mDuration;
		float	mValueBegin, mValueEnd;
		RampFn	mRampFn;
		bool	mMarkedForRemoval;

		// debug
		size_t mTotalFrames, mFramesProcessed;
	};

	void					initInternalBuffer();

	std::list<Event>	mEvents;

	ContextRef	mContext;
	NodeRef		mModulatorNode;
	float		mValue;

	BufferDynamic	mInternalBuffer;
};

} } // namespace cinder::audio2