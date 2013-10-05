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

#if defined( CINDER_COCOA )
	#include "audio2/cocoa/CinderCoreAudio.h"
#endif

using namespace std;
using namespace ci;

namespace cinder { namespace audio2 {

unique_ptr<Converter> Converter::create( const Format &sourceFormat, const Format &destFormat )
{
#if defined( CINDER_COCOA )
	return unique_ptr<Converter>( new cocoa::ConverterImplCoreAudio( sourceFormat, destFormat ) );
#endif
	return unique_ptr<Converter>();
}

Converter::Converter( const Format &sourceFormat, const Format &destFormat )
	: mSourceFormat( sourceFormat ), mDestFormat( destFormat )
{
	CI_ASSERT( mSourceFormat.getChannels() && mSourceFormat.getSampleRate() && mSourceFormat.getFramesPerBlock() );

	if( ! mDestFormat.getChannels() )
		mDestFormat.channels( mSourceFormat.getChannels() );
	if( ! mDestFormat.getSampleRate() )
		mDestFormat.channels( mSourceFormat.getChannels() );

}

} } // namespace cinder::audio2