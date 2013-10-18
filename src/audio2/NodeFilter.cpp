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

#include "audio2/NodeFilter.h"

using namespace std;

namespace cinder { namespace audio2 {

NodeFilterLowPass::NodeFilterLowPass( const Format &format )
	: NodeEffect( format ), mCoeffsDirty( true ), mCutoffFreq( 200.0f ), mResonance( 3.0f )
{
}

void NodeFilterLowPass::initialize()
{
	// Convert from Hertz to normalized frequency 0 -> 1.
	mNiquist = getContext()->getSampleRate() / 2;

	mBufferd = BufferT<double>( getContext()->getFramesPerBlock(), mNumChannels );
	mBiquads.resize( mNumChannels );

	if( mCoeffsDirty )
		updateBiquadParams();
}

void NodeFilterLowPass::uninitialize()
{
	mBiquads.clear();
}

void NodeFilterLowPass::process( Buffer *buffer )
{
	if( mCoeffsDirty )
		updateBiquadParams();

	size_t numFrames = buffer->getNumFrames();

	for( size_t ch = 0; ch < mNumChannels; ch++ ) {
		float *channel = buffer->getChannel( ch );
		mBiquads[ch].process( channel, channel, numFrames );
	}

}

void NodeFilterLowPass::updateBiquadParams()
{
	mCoeffsDirty = false;

	double normalizedFrequency = mCutoffFreq / mNiquist;
	for( size_t ch = 0; ch < mNumChannels; ch++ ) {
		mBiquads[ch].setLowpassParams( normalizedFrequency, mResonance );
	}
}


} } // namespace cinder::audio2