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

//! Array-based linear ramping function.
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

//! Array-based exponential ramping function.
void rampExpo( float *array, size_t count, float valueBegin, float valueEnd, float timeBeginNormalized, float timeEndNormalized )
{
	float timeIncr = ( timeEndNormalized - timeBeginNormalized ) / (float)count;
	float t = timeBeginNormalized;
	for( size_t i = 0; i < count; i++ ) {
		float valueNormalized = t * t;
		float valueScaled = valueBegin * ( 1 - valueNormalized ) + valueEnd * valueNormalized;
		array[i] = valueScaled;

		t += timeIncr;
	}
}


Param::Event::Event( float timeBegin, float timeEnd, float valueBegin, float valueEnd, const RampFn &rampFn )
	: mTimeBegin( timeBegin ), mTimeEnd( timeEnd ), mTotalSeconds( timeEnd - timeBegin ),
	mValueBegin( valueBegin ), mValueEnd( valueEnd ), mRampFn( rampFn ), mMarkedForRemoval( false )
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

void Param::rampTo( float endValue, float rampSeconds, const Options &options )
{
	CI_ASSERT( mContext );

	if( ! mInternalBufferInitialized ) {
		mInternalBuffer.resize( mContext->getFramesPerBlock() );
		mInternalBufferInitialized = true;
	}

	float timeBegin = mContext->getNumProcessedSeconds() + options.getDelay();
	float timeEnd = timeBegin + rampSeconds;

	Event event( timeBegin, timeEnd, mValue, endValue, options.getRampFn() );

	// debug
	event.mTotalFrames = event.mTotalSeconds * mContext->getSampleRate();
	LOG_V << "(rampTo) event time: " << event.mTimeBegin << "-" << event.mTimeEnd << " (" << event.mTotalSeconds << "), val: " << mValue << " - " << endValue << ", ramp seconds: " << rampSeconds << ", delay: " << options.getDelay() << endl;

	lock_guard<mutex> lock( mContext->getMutex() );

	reset();
	mEvents.push_back( event );
}

void Param::rampTo( float beginValue, float endValue, float rampSeconds, const Options &options )
{
	mValue = beginValue;
	rampTo( endValue, rampSeconds, options );
}

void Param::appendTo( float endValue, float rampSeconds, const Options &options )
{
	CI_ASSERT( mContext );

	if( ! mInternalBufferInitialized ) {
		mInternalBuffer.resize( mContext->getFramesPerBlock() );
		mInternalBufferInitialized = true;
	}

	float timeBegin = findEndTime() + options.getDelay();
	float timeEnd = timeBegin + rampSeconds;

	Event event( timeBegin, timeEnd, mValue, endValue, options.getRampFn() );

	// debug
	event.mTotalFrames = event.mTotalSeconds * mContext->getSampleRate();
	LOG_V << "event time: " << event.mTimeBegin << "-" << event.mTimeEnd << " (" << event.mTotalSeconds << "), val: " << mValue << " - " << endValue << ", ramp seconds: " << rampSeconds << ", delay: " << options.getDelay() << endl;

	lock_guard<mutex> lock( mContext->getMutex() );

	mEvents.push_back( event );
}

void Param::reset()
{
	if( mEvents.empty() )
		return;

	mEvents.clear();
}

bool Param::isVaryingThisBlock() const
{
	CI_ASSERT( mContext );

	for( const Event &event : mEvents ) {

		float timeBegin = mContext->getNumProcessedSeconds();
		float timeEnd = timeBegin + (float)mContext->getFramesPerBlock() / (float)mContext->getSampleRate();

//		if( event.mTimeBegin <= timeBegin || event.mTimeEnd >= timeEnd )
//			return true;

		if( event.mTimeBegin < timeEnd && event.mTimeEnd > timeBegin )
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

float Param::findEndTime()
{
	if( mEvents.empty() )
		return mContext->getNumProcessedSeconds();
	else
		return mEvents.back().mTimeEnd;
}

void Param::eval( float timeBegin, float *array, size_t arrayLength, size_t sampleRate )
{
	size_t samplesWritten = 0;
	
	for( Event &event : mEvents ) {

		float samplePeriod = 1.0f / sampleRate;
		float timeEnd = timeBegin + arrayLength * samplePeriod;

		if( event.mTimeBegin < timeEnd ) {
			if( event.mTimeEnd > timeBegin ) {
				size_t startIndex = timeBegin >= event.mTimeBegin ? 0 : size_t( ( event.mTimeBegin - timeBegin ) * sampleRate );
				size_t endIndex = timeEnd < event.mTimeEnd ? arrayLength : size_t( ( event.mTimeEnd - timeBegin ) * sampleRate );

				CI_ASSERT( startIndex <= arrayLength && endIndex <= arrayLength );

				if( startIndex > 0 && samplesWritten == 0 )
					fill( mValue, array, startIndex );

				size_t count = size_t( endIndex - startIndex );
				float timeBeginNormalized = float( timeBegin - event.mTimeBegin + startIndex * samplePeriod ) / event.mTotalSeconds;
				float timeEndNormalized = float( timeBegin - event.mTimeBegin + ( endIndex - 1 ) * samplePeriod ) / event.mTotalSeconds; // TODO: currently unused, needed?

				event.mRampFn( array + startIndex, count, event.mValueBegin, event.mValueEnd, timeBeginNormalized, timeEndNormalized );

				event.mFramesProcessed += count;

				samplesWritten += count;

				if( endIndex < arrayLength ) {
					mValue = event.mValueEnd;
					mEvents.pop_front();
					continue;
				}
			}
		}
		else {
			// context time end is > than event time begin, so remove
			mEvents.pop_front();
		}
	}

	// if after all events we still haven't written enough samples, fill with the final mValue, which
	// was updated above to be the last event's mValueEnd
	if( samplesWritten < arrayLength ) {
		size_t zeroLeft = size_t( arrayLength - samplesWritten );
		size_t offset = (size_t)samplesWritten;
		fill( mValue, array + offset, zeroLeft );
	}
	else
		mValue = array[arrayLength - 1];
}

} } // namespace cinder::audio2