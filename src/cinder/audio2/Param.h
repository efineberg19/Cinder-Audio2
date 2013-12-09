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

//! Array-based linear ramping function.
void rampLinear( float *array, size_t count, float t, float tIncr, const std::pair<float, float> &valueRange );
//! Array-based quadradic (t^2) ease-in ramping function.
void rampInQuad( float *array, size_t count, float t, float tIncr, const std::pair<float, float> &valueRange );
//! Array-based quadradic (t^2) ease-out ramping function.
void rampOutQuad( float *array, size_t count, float t, float tIncr, const std::pair<float, float> &valueRange );

class Param {
  public:
	//! note: unless we want to add _VARIADIC_MAX=6 in preprocessor definitions to all projects, number of args here has to be 5 or less for vc11 support
	typedef std::function<void ( float *, size_t, float, float, const std::pair<float, float>& )>	RampFn;

	Param( Node *parentNode, float initialValue = 0.0f );

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

	//! Sets the value of the Param, blowing away any scheduled Event's or modulator. \note Must be called from a non-audio thread.
	void	setValue( float value );
	//! Returns the current value of the Param.
	float	getValue() const	{ return mValue; }
	//! Returns a pointer to the buffer used when evaluating a Param that is varying over the current processing block, of equal size to the owning Context's frames per block.
	//! \note If not varying (eval() returns false), the returned pointer will be invalid.
	float*	getValueArray();

	//! Replaces any existing events with a ramp event from the current value to \a endValue over \a rampSeconds, according to \a options. If there is an existing modulator, it is disconnected.
	void applyRamp( float endValue, float rampSeconds, const Options &options = Options() );
	//! Replaces any existing ramps param manipulations with a ramp event from \a beginValue to \a endValue over \a rampSeconds, according to \a options. If there is an existing modulator, it is disconnected.
	void applyRamp( float beginValue, float endValue, float rampSeconds, const Options &options = Options() );
	//! Appends a ramp event from the end of the last scheduled event (or the current time) to \a endValue over \a rampSeconds, according to \a options. If there is an existing modulator, it is disconnected.
	void appendRamp( float endValue, float rampSeconds, const Options &options = Options() );

	//! Sets this Param's input to be the processing performed by \a node, blowing away any scheduled Event's. \note Forces \a node to be mono.
	void setModulator( const NodeRef &node );

	//! Resets Param, blowing away any Event's or modulator. \note Must be called from a non-audio thread.
	void reset();
	//! Returns the number of Event's that are currently scheduled.
	size_t getNumEvents() const;

	//! Evaluates the Param's events for the current processing block, determined from the parent Node's Context.
	//! \return true if the Param is varying this block and getValueArray() should be used, or false if the Param's value is constant for this block (use getValue()).
	bool	eval();
	//! Evaluates the Param's events from \a timeBegin for \a arrayLength samples at \a sampleRate.
	//! \return true if the Param is varying this block and getValueArray() should be used, or false if the Param's value is constant for this block (use getValue()).
	bool	eval( float timeBegin, float *array, size_t arrayLength, size_t sampleRate );

	//! Returns the total duration of any schedulated events (including delays), or 0 if none are scheduled.
	float					findDuration() const;
	//! Returns the end time and value of the latest scheduled event, or [0,getValue()] if none are scheduled.
	std::pair<float, float> findEndTimeAndValue() const;

  protected:
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

	// non-locking protected methods
	void		initInternalBuffer();
	void		resetImpl();
	ContextRef	getContext() const;


	std::list<Event>	mEvents;

	Node*		mParentNode;
	NodeRef		mModulator;
	float		mValue;

	BufferDynamic	mInternalBuffer;
};

} } // namespace cinder::audio2