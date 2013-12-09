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

#include "cinder/audio2/Param.h"
#include "cinder/audio2/Context.h"
#include "cinder/audio2/dsp/Dsp.h"
#include "cinder/audio2/Debug.h"

#include "cinder/CinderMath.h"

using namespace std;

namespace cinder { namespace audio2 {

void rampLinear( float *array, size_t count, float t, float tIncr, const std::pair<float, float> &valueRange )
{
	for( size_t i = 0; i < count; i++ ) {
		float factor = t;
		array[i] = lerp( valueRange.first, valueRange.second, factor );
		t += tIncr;
	}
}

void rampInQuad( float *array, size_t count, float t, float tIncr, const std::pair<float, float> &valueRange )
{
	for( size_t i = 0; i < count; i++ ) {
		float factor = t * t;
		array[i] = lerp( valueRange.first, valueRange.second, factor );
		t += tIncr;
	}
}

void rampOutQuad( float *array, size_t count, float t, float tIncr, const std::pair<float, float> &valueRange )
{
	for( size_t i = 0; i < count; i++ ) {
		float factor = -t * ( t - 2 );
		array[i] = lerp( valueRange.first, valueRange.second, factor );
		t += tIncr;
	}
}

Param::Event::Event( float timeBegin, float timeEnd, float valueBegin, float valueEnd, const RampFn &rampFn )
	: mTimeBegin( timeBegin ), mTimeEnd( timeEnd ), mDuration( timeEnd - timeBegin ),
	mValueBegin( valueBegin ), mValueEnd( valueEnd ), mRampFn( rampFn ), mMarkedForRemoval( false )
{
	mFramesProcessed = 0;
}

void Param::initialize( const ContextRef &context )
{
	mContext = context;
}

void Param::setValue( float value )
{
//	lock_guard<mutex> lock( mContext->getMutex() );

	reset();
	mValue = value;
}

void Param::applyRamp( float endValue, float rampSeconds, const Options &options )
{
	applyRamp( mValue, endValue, rampSeconds, options );
}

void Param::applyRamp( float beginValue, float endValue, float rampSeconds, const Options &options )
{
	initInternalBuffer();

	float timeBegin = (float)mContext->getNumProcessedSeconds() + options.getDelay();
	float timeEnd = timeBegin + rampSeconds;

	Event event( timeBegin, timeEnd, beginValue, endValue, options.getRampFn() );

	// debug
	event.mTotalFrames = size_t( event.mDuration * mContext->getSampleRate() );
//	LOG_V << "event time: " << event.mTimeBegin << "-" << event.mTimeEnd << " (" << event.mDuration << "), val: " << event.mValueBegin << " - " << event.mValueEnd << ", rampSeconds: " << rampSeconds << ", delay: " << options.getDelay() << endl;

	lock_guard<mutex> lock( mContext->getMutex() );

	reset();
	mEvents.push_back( event );
}

void Param::appendRamp( float endValue, float rampSeconds, const Options &options )
{
	initInternalBuffer();

	auto endTimeAndValue = findEndTimeAndValue();

	float timeBegin = endTimeAndValue.first + options.getDelay();
	float timeEnd = timeBegin + rampSeconds;

	Event event( timeBegin, timeEnd, endTimeAndValue.second, endValue, options.getRampFn() );

	// debug
	event.mTotalFrames = size_t( event.mDuration * mContext->getSampleRate() );
//	LOG_V << "event time: " << event.mTimeBegin << "-" << event.mTimeEnd << " (" << event.mDuration << "), val: " << event.mValueBegin << " - " << event.mValueEnd << ", rampSeconds: " << rampSeconds << ", delay: " << options.getDelay() << endl;

	lock_guard<mutex> lock( mContext->getMutex() );

	mEvents.push_back( event );
}

void Param::setModulator( const NodeRef &node )
{
	if( ! node )
		return;

	initInternalBuffer();

	CI_ASSERT( mContext );
	lock_guard<mutex> lock( mContext->getMutex() );

	reset();

	// FIXME: node doesn't know that its output is Param, and as such is mono.
	
	node->initializeImpl();

	mModulatorNode = node;

	LOG_V( "modulator to: " << mModulatorNode->getTag() );
}

void Param::reset()
{
	if( mEvents.empty() )
		return;

	mEvents.clear();
}


size_t Param::getNumEvents() const
{
	lock_guard<mutex> lock( mContext->getMutex() );

	return mEvents.size();
}

bool Param::isVaryingThisBlock() const
{
	CI_ASSERT( mContext );

	if( mModulatorNode )
		return true;

	for( const Event &event : mEvents ) {

		float timeBegin = (float)mContext->getNumProcessedSeconds();
		float timeEnd = timeBegin + (float)mContext->getFramesPerBlock() / (float)mContext->getSampleRate();

		if( event.mTimeBegin < timeEnd && event.mTimeEnd > timeBegin )
			return true;
	}
	return false;
}

float* Param::getValueArray()
{
	CI_ASSERT( mContext );

	if( mModulatorNode )
		mModulatorNode->pullInputs( &mInternalBuffer );
	else
		eval( (float)mContext->getNumProcessedSeconds(), mInternalBuffer.getData(), mInternalBuffer.getSize(), mContext->getSampleRate() );

	return mInternalBuffer.getData();
}

float Param::findDuration() const
{
	lock_guard<mutex> lock( mContext->getMutex() );

	if( mEvents.empty() )
		return 0;
	else {
		const Event &event = mEvents.back();
		return event.mTimeEnd - (float)mContext->getNumProcessedSeconds();
	}
}

pair<float, float> Param::findEndTimeAndValue() const
{
	lock_guard<mutex> lock( mContext->getMutex() );

	if( mEvents.empty() )
		return make_pair( (float)mContext->getNumProcessedSeconds(), mValue );
	else {
		const Event &event = mEvents.back();
		return make_pair( event.mTimeEnd, event.mValueEnd );
	}
}

void Param::eval( float timeBegin, float *array, size_t arrayLength, size_t sampleRate )
{
	size_t samplesWritten = 0;
	const float samplePeriod = 1.0f / sampleRate;

	for( Event &event : mEvents ) {
		// if event time end is before the current processing block, remove event
		if( event.mTimeEnd < timeBegin ) {
			mEvents.pop_front();
			continue;
		}

		float timeEnd = timeBegin + arrayLength * samplePeriod;

		if( event.mTimeBegin < timeEnd && event.mTimeEnd > timeBegin ) {
			size_t startIndex = timeBegin >= event.mTimeBegin ? 0 : size_t( ( event.mTimeBegin - timeBegin ) * sampleRate );
			size_t endIndex = timeEnd < event.mTimeEnd ? arrayLength : size_t( ( event.mTimeEnd - timeBegin ) * sampleRate );

			CI_ASSERT( startIndex <= arrayLength && endIndex <= arrayLength );

			if( startIndex > 0 && samplesWritten == 0 )
				dsp::fill( mValue, array, startIndex );

			size_t count = size_t( endIndex - startIndex );
			float timeBeginNormalized = float( timeBegin - event.mTimeBegin + startIndex * samplePeriod ) / event.mDuration;
			float timeEndNormalized = float( timeBegin - event.mTimeBegin + endIndex * samplePeriod ) / event.mDuration;
			float timeIncr = ( timeEndNormalized - timeBeginNormalized ) / (float)count;

			event.mRampFn( array + startIndex, count, timeBeginNormalized, timeIncr, make_pair( event.mValueBegin, event.mValueEnd ) );

			// debug
			event.mFramesProcessed += count;

			samplesWritten += count;

			// if this event ended with the current processing block, update mValue then remove event
			if( endIndex < arrayLength ) {
				mValue = event.mValueEnd;
				mEvents.pop_front();
			}
			else if( samplesWritten == arrayLength )
				break;
		}
	}

	// if after all events we still haven't written enough samples, fill with the final mValue, which
	// was updated above to be the last event's mValueEnd. else set mValue to the last updated array value
	if( samplesWritten < arrayLength )
		dsp::fill( mValue, array + (size_t)samplesWritten, size_t( arrayLength - samplesWritten ) );
	else
		mValue = array[arrayLength - 1];
}

void Param::initInternalBuffer()
{
	CI_ASSERT( mContext );

	if( mInternalBuffer.isEmpty() )
		mInternalBuffer.setNumFrames( mContext->getFramesPerBlock() );
}

} } // namespace cinder::audio2