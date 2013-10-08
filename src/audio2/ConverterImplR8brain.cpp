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

#include "audio2/ConverterImplR8brain.h"
#include "r8brain/CDSPResampler.h"

#include <algorithm>

using namespace std;
using namespace ci;

namespace cinder { namespace audio2 {

// Notes on how to make r8brain run a little faster, if needed:
// For a fair comparison you may also decrease ReqTransBand and increase ReqAtten if necessary - this won't make r8brain-free-src much slower.
// http://www.kvraudio.com/forum/viewtopic.php?t=389711&postdays=0&postorder=asc&start=30
//
//	The basic formula for ReqAtten is something close to 6.02*BitDepth+40. The ReqTransBand selection depends on how "greedy" you are for the highest frequencies. It's set to 2% by default, but in practice you can use 4 or 5, that still leaves a lot of frequency content (flat up to 21kHz for 44.1k audio).

ConverterImplR8brain::ConverterImplR8brain( const Format &sourceFormat, const Format &destFormat )
	: Converter( sourceFormat, destFormat )
{
	size_t numResamplers;
	if( mSourceFormat.getChannels() > mDestFormat.getChannels() ) {
		// downmixing, resample dest channels -> source channels
		numResamplers = mDestFormat.getChannels();
		mMixingBuffer = Buffer( sourceFormat.getFramesPerBlock(), destFormat.getChannels() );
	}
	else if( mSourceFormat.getChannels() < mDestFormat.getChannels() ) {
		// upmixing, resample source channels
		size_t destFramesPerBlock = ceil( (float)sourceFormat.getFramesPerBlock() * (float)destFormat.getSampleRate() / (float)sourceFormat.getSampleRate() );
		numResamplers = mSourceFormat.getChannels();
		mMixingBuffer = Buffer( destFramesPerBlock, sourceFormat.getChannels() );
	}
	else
		numResamplers = mSourceFormat.getChannels();

	mBufferd = BufferT<double>( sourceFormat.getFramesPerBlock(), numResamplers );

	for( size_t ch = 0; ch < numResamplers; ch++ )
		mResamplers.push_back( unique_ptr<r8b::CDSPResampler24>( new r8b::CDSPResampler24( (const double)mSourceFormat.getSampleRate(), (const double)mDestFormat.getSampleRate(), (const int)mSourceFormat.getFramesPerBlock() ) ) );
}

ConverterImplR8brain::~ConverterImplR8brain()
{
}

// TODO: assert all params are possible
std::pair<size_t, size_t> ConverterImplR8brain::convert( const Buffer *sourceBuffer, Buffer *destBuffer )
{
	if( mSourceFormat.getChannels() == mDestFormat.getChannels() )
		convertImpl( sourceBuffer, destBuffer );
	else if( mSourceFormat.getChannels() > mDestFormat.getChannels() )
		return convertImplDownMixing( sourceBuffer, destBuffer );

	return convertImplUpMixing( sourceBuffer, destBuffer );
}

std::pair<size_t, size_t> ConverterImplR8brain::convertImpl( const Buffer *sourceBuffer, Buffer *destBuffer )
{
	mBufferd.copy( *sourceBuffer );

	int readCount = (int)mBufferd.getNumFrames();
	int outCount = 0;
	for( size_t ch = 0; ch < mBufferd.getNumChannels(); ch++ ) {
		double *out = nullptr;
		outCount = mResamplers[ch]->process( mBufferd.getChannel( ch ), readCount, out );
		copy( out, out + outCount, destBuffer->getChannel( ch ) );
	}

	return make_pair( readCount, (size_t)outCount );
}

std::pair<size_t, size_t> ConverterImplR8brain::convertImplDownMixing( const Buffer *sourceBuffer, Buffer *destBuffer )
{
	submixBuffers( sourceBuffer, &mMixingBuffer );
	mBufferd.copy( mMixingBuffer );

	int readCount = (int)mBufferd.getNumFrames();
	int outCount = 0;
	for( size_t ch = 0; ch < mBufferd.getNumChannels(); ch++ ) {
		double *out = nullptr;
		outCount = mResamplers[ch]->process( mBufferd.getChannel( ch ), readCount, out );
		copy( out, out + outCount, destBuffer->getChannel( ch ) );
	}

	return make_pair( readCount, (size_t)outCount );
}

// FIXME: got the wobbles...
std::pair<size_t, size_t> ConverterImplR8brain::convertImplUpMixing( const Buffer *sourceBuffer, Buffer *destBuffer )
{
	mBufferd.copy( *sourceBuffer );

	int readCount = (int)mBufferd.getNumFrames();
	int outCount = 0;
	for( size_t ch = 0; ch < mBufferd.getNumChannels(); ch++ ) {
		double *out = nullptr;
		outCount = mResamplers[ch]->process( mBufferd.getChannel( ch ), readCount, out );
		copy( out, out + outCount, mMixingBuffer.getChannel( ch ) );
	}

	submixBuffers( &mMixingBuffer, destBuffer );

	return make_pair( readCount, (size_t)outCount );
}

} } // namespace cinder::audio2