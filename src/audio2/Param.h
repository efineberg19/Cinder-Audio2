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

#include <vector>
#include <functional>

namespace cinder { namespace audio2 {

typedef std::shared_ptr<class Context>			ContextRef;

void rampLinear( float *array, size_t count, float valueBegin, float valueEnd, float timeBeginNormalized, float timeEndNormalized );
void rampExpo( float *array, size_t count, float valueBegin, float valueEnd, float timeBeginNormalized, float timeEndNormalized );

class Param {
  public:
	typedef std::function<void ( float *, size_t, float, float, float, float )>	RampFn;

	explicit Param( float initialValue = 0.0f ) : mValue( initialValue ), mInternalBufferInitialized( false ) {}

	void initialize( const ContextRef &context );

	float	getValue() const	{ return mValue; }
	void	setValue( float value );

	void rampTo( float value, float rampSeconds, const RampFn &rampFn = &rampLinear )		{ rampTo( value, rampSeconds, 0.0 ); }
	void rampTo( float value, double rampSeconds, double delaySeconds, const RampFn &rampFn = &rampLinear );

	bool	isVaryingThisBlock() const;

	float*	getValueArray();
	void	eval( float timeBegin, float *array, size_t arrayLength, size_t sampleRate );

  private:
	struct Event {
		Event() {}
		Event( float timeBegin, float timeEnd, float valueBegin, float valueEnd, const RampFn &rampFn );

		float	mTimeBegin, mTimeEnd, mTotalSeconds;
		float	mValueBegin, mValueEnd;
		RampFn	mRampFn;
		bool	mMarkedForRemoval;

		// debug
		size_t mTotalFrames, mFramesProcessed;
	};

	std::vector<Event>	mEvents;

	ContextRef	mContext;
	float		mValue;

	bool				mInternalBufferInitialized;
	std::vector<float>	mInternalBuffer;
};

} } // namespace cinder::audio2