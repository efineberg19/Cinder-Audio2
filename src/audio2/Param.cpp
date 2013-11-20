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

#include "audio2/Param.h"
#include "audio2/Context.h"
#include "audio2/Dsp.h"
#include "audio2/Debug.h"

using namespace std;

namespace cinder { namespace audio2 {

Param::Event::Event( float timeBegin, float timeEnd, float valueBegin, float valueEnd )
	: mTimeBegin( timeBegin ), mTimeEnd( timeEnd ), mTotalSeconds( timeEnd - timeBegin ),
	mValueBegin( valueBegin ), mValueEnd( valueEnd ), mMarkedForRemoval( false )
{
}

void Param::initialize( const ContextRef &context )
{
	mContext = context;
}

void Param::setValue( float value )
{
	mValue = value;
}

void Param::rampTo( float value, double rampSeconds, double delaySeconds )
{
	CI_ASSERT( mContext );

	if( ! mInternalBufferInitialized )
		mInternalBuffer.resize( mContext->getFramesPerBlock() );

	float timeBegin = mContext->getNumProcessedSeconds() + delaySeconds;
	float timeEnd = timeBegin + rampSeconds;

	Event event( timeBegin, timeEnd, mValue, value );

	// debug
	event.mTotalFrames = event.mTotalSeconds * mContext->getSampleRate();
	event.mFramesProcessed = 0;

//	app::console() << "event time: " << event.mTimeBegin << "-" << event.mTimeEnd << " (" << event.mTotalSeconds << "), val: " << mValue << " - " << value << ", ramp seconds: " << rampSeconds << ", delay: " << delaySeconds << endl;

	lock_guard<mutex> lock( mContext->getMutex() );

	mEvents.resize( 1 );
	mEvents[0] = event;
}

bool Param::isVaryingThisBlock() const
{
	CI_ASSERT( mContext );

	if( ! mEvents.empty() ) {

		const Event &event = mEvents[0];

		float timeBegin = mContext->getNumProcessedSeconds();
		float timeEnd = timeBegin + mContext->getFramesPerBlock() / mContext->getSampleRate();

		if( event.mTimeBegin <= timeBegin || event.mTimeEnd >= timeEnd )
			return true;
	}
	return false;
}

float* Param::getValueArray()
{
	CI_ASSERT( mContext );

	eval( mContext->getNumProcessedSeconds(), mInternalBuffer.data(), mInternalBuffer.size(), mContext->getSampleRate() );
	return mInternalBuffer.data();
}

namespace {

//! Array-based linear ramping function. \a valueBegin and \a valueEnd are the complete values from the Event, while \a timeBegin and \a timeEnd are normalized values between the tweens total time length
void rampLinear( float *array, size_t count, float valueBegin, float valueEnd, float timeBeginNormalized, float timeEndNormalized )
{
	float timeIncr = ( timeEndNormalized - timeBeginNormalized ) / (float)count;
	float t = timeBeginNormalized;
	for( size_t i = 0; i < count; i++ ) {
		float valueNormalized = t;
		float valueScaled = valueBegin * ( 1 - valueNormalized ) + valueEnd * valueNormalized;
		array[i] = valueScaled;

		t += timeIncr;
	}
}

}

void Param::eval( float timeBegin, float *array, size_t arrayLength, size_t sampleRate )
{
	if( mEvents.empty() ) {
		fill( mValue, array, arrayLength );
		return;
	}

	Event &event = mEvents[0];

	float samplePeriod = 1.0f / sampleRate;
	float timeEnd = timeBegin + arrayLength * samplePeriod;

	if( timeEnd < event.mTimeBegin ) {
		// event does not begin until after this block, so just fill array with current value
		fill( mValue, array, arrayLength );
	}
	else if( timeBegin < event.mTimeEnd ) {
		size_t startIndex = timeBegin >= event.mTimeBegin ? 0 : size_t( ( event.mTimeBegin - timeBegin ) * sampleRate );
		size_t endIndex = timeEnd < event.mTimeEnd ? arrayLength : size_t( ( event.mTimeEnd - timeBegin ) * sampleRate );

		CI_ASSERT( startIndex <= arrayLength && endIndex <= arrayLength );

		if( startIndex > 0 )
			fill( mValue, array, startIndex );

		size_t count = size_t( endIndex - startIndex );
		float timeBeginNormalized = float( timeBegin - event.mTimeBegin + startIndex * samplePeriod ) / event.mTotalSeconds;
		float timeEndNormalized = float( timeBegin - event.mTimeBegin + ( endIndex - 1 ) * samplePeriod ) / event.mTotalSeconds; // TODO: currently unused, needed?

		rampLinear( array + startIndex, count, event.mValueBegin, event.mValueEnd, timeBeginNormalized, timeEndNormalized );

		event.mFramesProcessed += count;

		if( endIndex < arrayLength ) {
			mValue = event.mValueEnd;
			size_t zeroLeft = size_t( arrayLength - endIndex );
			size_t offset = (size_t)endIndex;
			fill( mValue, array + offset, zeroLeft );
			event.mMarkedForRemoval = true;
		}
		else
			mValue = array[arrayLength - 1];
	}
	else
		event.mMarkedForRemoval = true;

	if( event.mMarkedForRemoval )
		mEvents.clear();
}

} } // namespace cinder::audio2