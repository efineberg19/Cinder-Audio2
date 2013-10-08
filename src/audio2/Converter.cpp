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

#include "audio2/Converter.h"
#include "audio2/CinderAssert.h"
#include "audio2/ConverterImplR8brain.h"

#if defined( CINDER_COCOA )
	#include "audio2/cocoa/CinderCoreAudio.h"
#endif


using namespace std;
using namespace ci;

namespace cinder { namespace audio2 {

unique_ptr<Converter> Converter::create( size_t sourceSampleRate, size_t destSampleRate, size_t sourceNumChannels, size_t destNumChannels, size_t sourceMaxFramesPerBlock )
{
#if defined( CINDER_COCOA )
	return unique_ptr<Converter>( new cocoa::ConverterImplCoreAudio( sourceSampleRate, destSampleRate, sourceNumChannels, destNumChannels, sourceMaxFramesPerBlock ) );
#else
	return unique_ptr<Converter>( new ConverterImplR8brain( sourceSampleRate, destSampleRate, sourceNumChannels, destNumChannels, sourceMaxFramesPerBlock ) );
#endif
}

Converter::Converter( size_t sourceSampleRate, size_t destSampleRate, size_t sourceNumChannels, size_t destNumChannels, size_t sourceMaxFramesPerBlock )
	: mSourceSampleRate( sourceSampleRate ), mDestSampleRate( destSampleRate ), mSourceNumChannels( sourceNumChannels ), mDestNumChannels( destNumChannels ), mSourceMaxFramesPerBlock( sourceMaxFramesPerBlock )
{
	CI_ASSERT( mSourceSampleRate && mSourceNumChannels && mSourceMaxFramesPerBlock );

	if( ! mDestSampleRate )
		mDestSampleRate = mSourceSampleRate;
	if( ! mDestNumChannels )
		mDestNumChannels = mSourceNumChannels;

	mDestMaxFramesPerBlock = std::ceil( (float)mSourceMaxFramesPerBlock * (float)mDestSampleRate / (float)mSourceSampleRate );
}

void Converter::submixBuffers( const Buffer *sourceBuffer, Buffer *destBuffer )
{
	CI_ASSERT( sourceBuffer->getNumFrames() == destBuffer->getNumFrames() );

	size_t sourceChannels = sourceBuffer->getNumChannels();
	size_t destChannels = destBuffer->getNumChannels();
	if( destChannels == sourceBuffer->getNumChannels() ) {
		for( size_t c = 0; c < destChannels; c++ )
			sum( destBuffer->getChannel( c ), sourceBuffer->getChannel( c ), destBuffer->getChannel( c ), destBuffer->getNumFrames() );
	}
	else if( sourceChannels == 1 ) {
		// up-mix mono sourceBuffer to destChannels
		for( size_t c = 0; c < destChannels; c++ )
			sum( destBuffer->getChannel( c ), sourceBuffer->getChannel( 0 ), destBuffer->getChannel( c ), destBuffer->getNumFrames() );
	}
	else if( destChannels == 1 ) {
		// down-mix mono destBuffer to sourceChannels
		// TODO: try equal power fading all channels to center
		for( size_t c = 0; c < sourceChannels; c++ )
			sum( destBuffer->getChannel( 0 ), sourceBuffer->getChannel( c ), destBuffer->getChannel( 0 ), destBuffer->getNumFrames() );
	}
	else
		CI_ASSERT( 0 && "unhandled" );
}

} } // namespace cinder::audio2