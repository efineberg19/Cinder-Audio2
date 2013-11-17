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

#include <vector>

namespace cinder { namespace audio2 {

class Param {
  public:
	Param( float initialValue = 0.0f ) : mValue( initialValue ), mDefaultRampSeconds( 0.005 ), mInternalBufferInitialized( false ) {}

	void initialize( const ContextRef &context );

	float	getValue() const	{ return mValue; }
	void	setValue( float value );

	void rampTo( float value )	{ rampTo( value, mDefaultRampSeconds ); }

	void rampTo( float value, double rampSeconds );

	void setDefaultRampSeconds( double seconds )	{ mDefaultRampSeconds = seconds; }

	bool	isVaryingNextEval() const;

	float*	getValueArray();
	void	eval( uint64_t beginFrame, uint64_t endFrame, float *array, size_t arrayLength, size_t sampleRate );

  private:
	struct Event {
		Event() {}
		Event( uint64_t beginFrame, uint64_t endFrame, double totalSeconds, float endValue );
		uint64_t mBeginFrame, mEndFrame;
		double mTotalSeconds;
		float mEndValue;

		// linear interpolation specific params (will be removed)
		float mIncr;
		uint64_t mFramesToProcess;
	};

	std::vector<Event>	mEvents;

	ContextRef	mContext;
	float		mValue;
	double		mDefaultRampSeconds;

	bool				mInternalBufferInitialized;
	std::vector<float>	mInternalBuffer;
};

} } // namespace cinder::audio2