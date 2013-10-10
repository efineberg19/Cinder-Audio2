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

NodeEffect::NodeEffect( const Format &format )
	: Node( format )
{
	if( boost::indeterminate( format.getAutoEnable() ) )
		setAutoEnabled();
}

void NodeGain::process( Buffer *buffer )
{
	multiply( buffer->getData(), mGain, buffer->getData(), buffer->getSize() );
}

// TODO: this is the first case where it makes sense to have # input channels != # output channels - worth supporting?

NodePan2d::NodePan2d( const Format &format )
: NodeEffect( format ), mPos( 0.5f )
{
	mChannelMode = ChannelMode::SPECIFIED;
	setNumChannels( 2 );
}

void NodePan2d::setPos( float pos )
{
	mPos = math<float>::clamp( pos );
}

// equal power panning eq:
// left = cos(p) * signal, right = sin(p) * signal, where p is in radians from 0 to PI/2
// gives +3db when panned to center, which helps to remove the 'dead spot'
//
// FIXME: this isn't appropriate for stereo panning, its just silencing one channel and blasting the other
void NodePan2d::process( Buffer *buffer )
{
	float posRadians = mPos * M_PI / 2.0f;
	float *leftChannel = buffer->getChannel( 0 );
	float *rightChannel = buffer->getChannel( 1 );

	multiply( leftChannel, std::cos( posRadians ), leftChannel, buffer->getNumFrames() );
	multiply( rightChannel, std::sin( posRadians ), rightChannel, buffer->getNumFrames() );
}

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