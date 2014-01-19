/*
 Copyright (c) 2014, The Cinder Project

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

#include "cinder/audio2/NodeInput.h"
#include "cinder/audio2/Context.h"
#include "cinder/audio2/Debug.h"

#include "cinder/Rand.h"

using namespace ci;
using namespace std;

namespace cinder { namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeInput
// ----------------------------------------------------------------------------------------------------

NodeInput::NodeInput( const Format &format ) : Node( format )
{
	mInputs.clear();

	// NodeInput's don't have inputs, so disallow matches input channels
	if( mChannelMode == ChannelMode::MATCHES_INPUT )
		mChannelMode = ChannelMode::MATCHES_OUTPUT;
}

NodeInput::~NodeInput()
{
}

void NodeInput::connectInput( const NodeRef &input, size_t bus )
{
	CI_ASSERT_MSG( 0, "NodeInput does not support inputs" );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - LineIn
// ----------------------------------------------------------------------------------------------------

LineIn::LineIn( const DeviceRef &device, const Format &format )
: NodeInput( format ), mDevice( device )
{
	if( boost::indeterminate( format.getAutoEnable() ) )
		setAutoEnabled();
}

LineIn::~LineIn()
{
}

// ----------------------------------------------------------------------------------------------------
// MARK: - CallbackProcessor
// ----------------------------------------------------------------------------------------------------

void CallbackProcessor::process( Buffer *buffer )
{
	if( mCallbackFn )
		mCallbackFn( buffer, getContext()->getSampleRate() );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - Gen's
// ----------------------------------------------------------------------------------------------------

Gen::Gen( const Format &format )
	: NodeInput( format ), mFreq( this ), mPhase( 0 )
{
	mChannelMode = ChannelMode::SPECIFIED;
	setNumChannels( 1 );
}

void Gen::initialize()
{
	mSampleRate = (float)getContext()->getSampleRate();
}

void GenNoise::process( Buffer *buffer )
{
	float *data = buffer->getData();
	size_t count = buffer->getSize();

	for( size_t i = 0; i < count; i++ )
		data[i] = ci::randFloat( -1.0f, 1.0f );
}

void GenSine::process( Buffer *buffer )
{
	float *data = buffer->getData();
	const size_t count = buffer->getSize();
	const float phaseMul = float( 2 * M_PI / (double)mSampleRate );
	float phase = mPhase;

	if( mFreq.eval() ) {
		float *freqValues = mFreq.getValueArray();
		for( size_t i = 0; i < count; i++ ) {
			data[i] = math<float>::sin( phase );
			phase = fmodf( phase + freqValues[i] * phaseMul, float( M_PI * 2 ) );
		}
	}
	else {
		const float phaseIncr = mFreq.getValue() * phaseMul;
		for( size_t i = 0; i < count; i++ ) {
			data[i] = math<float>::sin( phase );
			phase = fmodf( phase + phaseIncr, float( M_PI * 2 ) );
		}
	}

	mPhase = phase;
}

void GenPhasor::process( Buffer *buffer )
{
	float *data = buffer->getData();
	const size_t count = buffer->getSize();
	const float phaseMul = 1.0f / mSampleRate;
	float phase = mPhase;

	if( mFreq.eval() ) {
		float *freqValues = mFreq.getValueArray();
		for( size_t i = 0; i < count; i++ ) {
			data[i] = phase;
			phase = fmodf( phase + freqValues[i] * phaseMul, 1 );
		}
	}
	else {
		const float phaseIncr = mFreq.getValue() * phaseMul;
		for( size_t i = 0; i < count; i++ ) {
			data[i] = phase;
			phase = fmodf( phase + phaseIncr, 1 );
		}
	}

	mPhase = phase;
}

namespace {

inline float calcTriangleSignal( float phase, float upSlope, float downSlope )
{
	// if up slope = down slope = 1, signal ranges from 0 to 0.5. so normalize this from -1 to 1
	float signal = std::min( phase * upSlope, ( 1 - phase ) * downSlope );
	return signal * 4 - 1;
}

} // anonymous namespace

void GenTriangle::process( Buffer *buffer )
{
	const float phaseMul = 1.0f / mSampleRate;
	float *data = buffer->getData();
	size_t count = buffer->getSize();
	float phase = mPhase;

	if( mFreq.eval() ) {
		float *freqValues = mFreq.getValueArray();
		for( size_t i = 0; i < count; i++ )	{
			data[i] = calcTriangleSignal( phase, mUpSlope, mDownSlope );
			phase = fmodf( phase + freqValues[i] * phaseMul, 1.0f );
		}

	}
	else {
		const float phaseIncr = mFreq.getValue() * phaseMul;
		for( size_t i = 0; i < count; i++ )	{
			data[i] = calcTriangleSignal( phase, mUpSlope, mDownSlope );
			phase = fmodf( phase + phaseIncr, 1 );
		}
	}

	mPhase = phase;
}

} } // namespace cinder::audio2