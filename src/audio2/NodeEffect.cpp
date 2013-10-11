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

#include "audio2/NodeEffect.h"
#include "audio2/Debug.h"

#include "cinder/CinderMath.h"

using namespace ci;
using namespace std;

namespace cinder { namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeEffect
// ----------------------------------------------------------------------------------------------------

NodeEffect::NodeEffect( const Format &format )
	: Node( format )
{
	if( boost::indeterminate( format.getAutoEnable() ) )
		setAutoEnabled();
}

// ----------------------------------------------------------------------------------------------------
// MARK: - NodeGain
// ----------------------------------------------------------------------------------------------------

void NodeGain::process( Buffer *buffer )
{
	multiply( buffer->getData(), mGain, buffer->getData(), buffer->getSize() );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - NodePan2d
// ----------------------------------------------------------------------------------------------------

NodePan2d::NodePan2d( const Format &format )
: NodeEffect( format ), mPos( 0.5f ), mMonoInputMode( true )
{
	mChannelMode = ChannelMode::SPECIFIED;
	setNumChannels( 2 );
}

/*
 * TODO: below is an attempt at an optimization for (possibly many) mono input -> stereo output
 8	- I didn't get it to mesh well enough with Node::configureConnections, during various edge cases, so it is on the back burner for now..
bool NodePan2d::supportsInputNumChannels( size_t numChannels )
{
	if( numChannels == 1 ) {
		mMonoInputMode = true;
		mProcessInPlace = false;
		size_t framesPerBlock = getContext()->getFramesPerBlock();

		// internal buffer is mono (which will be passed to inputs), while summing buffer is stereo
		if( mInternalBuffer.getNumChannels() != 1 || mInternalBuffer.getNumFrames() != framesPerBlock )
			mInternalBuffer = Buffer( framesPerBlock, 1 );
		if( mSummingBuffer.getNumChannels() != mNumChannels || mSummingBuffer.getNumFrames() != framesPerBlock )
			mSummingBuffer = Buffer( framesPerBlock, mNumChannels );
	}
	else
		mMonoInputMode = false;

	LOG_V << "mono input mode: " << boolalpha << mMonoInputMode << dec << endl;

	return ( numChannels <= 2 );
}

void NodePan2d::pullInputs( Buffer *destBuffer )
{
	CI_ASSERT( getContext() );

	if( ! mMonoInputMode )
		Node::pullInputs( destBuffer );
	else {
		// inputs are summed to channel 0 only
		mInternalBuffer.zero();
		mSummingBuffer.zero();

		size_t numFrames = mSummingBuffer.getNumFrames();
		float *summingChannel0 = mSummingBuffer.getChannel( 0 );
		for( NodeRef &input : mInputs ) {
			if( ! input )
				continue;

			input->pullInputs( &mInternalBuffer );
			if( input->getProcessInPlace() )
				add( summingChannel0, mInternalBuffer.getChannel( 0 ), summingChannel0, numFrames );
			else
				add( summingChannel0, input->getInternalBuffer()->getChannel( 0 ), summingChannel0, numFrames );
		}

		if( mEnabled )
			process( &mSummingBuffer );

		// at this point, audio will be in both stereo channels

		// TODO: if possible, just copy summing buffer to output buffer
		// - this is on hold until further work towards avoiding both mInternalBuffer and mSummingBuffer
		// - at that point, it may be possible to avoid this mix as well, in some cases
		Converter::mixBuffers( &mSummingBuffer, destBuffer );
	}
}
*/

// equal power panning eq:
// left = cos(p) * signal, right = sin(p) * signal, where p is in radians from 0 to PI/2
// gives +3db when panned to center, which helps to remove the 'dead spot'
void NodePan2d::process( Buffer *buffer )
{
	float pos = mPos;
	float *channel0 = buffer->getChannel( 0 );
	float *channel1 = buffer->getChannel( 1 );

	float posRadians = pos * M_PI / 2.0f;
	float leftGain = std::cos( posRadians );
	float rightGain = std::sin( posRadians );

	if( mMonoInputMode ) {
		multiply( channel0, leftGain, channel0, buffer->getNumFrames() );
		multiply( channel1, rightGain, channel1, buffer->getNumFrames() );
	}
	else {

		// suitable impl for stereo panning an already-stereo input...

		static const float kCenterGain = std::cos( M_PI / 4.0f );

		size_t n = buffer->getNumFrames();
		if( pos < 0.5f ) {
			for( size_t i = 0; i < n; i++ ) {
				channel0[i] = channel0[i] * leftGain + channel1[i] * ( leftGain - kCenterGain );
				channel1[i] *= rightGain;
			}
		} else {
			for( size_t i = 0; i < n; i++ ) {
				channel1[i] = channel1[i] * rightGain + channel0[i] * ( rightGain - kCenterGain );
				channel0[i] *= leftGain;
			}
		}
	}
}

void NodePan2d::setPos( float pos )
{
	mPos = math<float>::clamp( pos );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - RingMod
// ----------------------------------------------------------------------------------------------------

void RingMod::process( Buffer *buffer )
{
	size_t numFrames = buffer->getNumFrames();
	if( mSineBuffer.size() < numFrames )
		mSineBuffer.resize( numFrames );
	mSineGen.process( &mSineBuffer );

	for ( size_t c = 0; c < buffer->getNumChannels(); c++ ) {
		float *channel = buffer->getChannel( c );
		for( size_t i = 0; i < numFrames; i++ )
			channel[i] *= mSineBuffer[i];
	}
}

} } // namespace cinder::audio2