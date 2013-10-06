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

ConverterImplR8brain::ConverterImplR8brain( const Format &sourceFormat, const Format &destFormat )
	: Converter( sourceFormat, destFormat ), mBufferd( sourceFormat.getFramesPerBlock(), sourceFormat.getChannels() )
{
	size_t numResamplers = min( mSourceFormat.getChannels(), mDestFormat.getChannels() );
	for( size_t ch = 0; ch < numResamplers; ch++ )
		mResamplers.push_back( unique_ptr<r8b::CDSPResampler24>( new r8b::CDSPResampler24( (const double)mSourceFormat.getSampleRate(), (const double)mDestFormat.getSampleRate(), (const int)mSourceFormat.getFramesPerBlock() ) ) );
}

ConverterImplR8brain::~ConverterImplR8brain()
{

}


// FIXME: occasional malloc errors once we go out of scope.
std::pair<size_t, size_t> ConverterImplR8brain::convert( const Buffer *sourceBuffer, Buffer *destBuffer )
{
	CI_ASSERT( sourceBuffer->getNumChannels() == destBuffer->getNumChannels() ); // TODO: channel conversion
	CI_ASSERT( sourceBuffer->getSize() == mBufferd.getSize() );

	mBufferd.copy( *sourceBuffer );

	int readCount = static_cast<int>( mBufferd.getNumFrames() );

	int outCount = 0;
	for( size_t ch = 0; ch < destBuffer->getNumChannels(); ch++ ) {
		double *out = nullptr;
		outCount = mResamplers[ch]->process( mBufferd.getChannel( ch ), readCount, out );
		cout << outCount << ", ";
		copy( out, out + outCount, destBuffer->getChannel( ch ) );
	}

	return make_pair( readCount, (size_t)outCount );
}

// TODO: make a simple test here that doesn't use audio2::Buffer
// - need to localize what is causing the malloc error
void ConverterImplR8brain::test()
{
	vector<double> inSamples( 512 );
	for( size_t i = 0; i < inSamples.size(); i++ )
		inSamples[i] = i + 1;
}

} } // namespace cinder::audio2