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

#pragma once

#include "cinder/audio2/Context.h"

#include <memory>

namespace cinder { namespace audio2 { namespace dsp {

class Converter {
public:

	//! If \a destSampleRate is 0, it is set to match \a sourceSampleRate. If \a destNumChannels is 0, it is set to match \a sourceNumChannels.
	static std::unique_ptr<Converter> create( size_t sourceSampleRate, size_t destSampleRate, size_t sourceNumChannels, size_t destNumChannels, size_t sourceMaxFramesPerBlock );

	virtual ~Converter() {}

	//! Returns a \a std::pair<num source frames used, num dest frames written>
	virtual std::pair<size_t, size_t> convert( const Buffer *sourceBuffer, Buffer *destBuffer ) = 0;

	//! Mixes \a sourceBuffer to \a destBuffer's layout, replacing its content. Channel up or down mixing is applied if necessary. Unequal frame counts are permitted (the minimum size will be used).
	static void mixBuffers( const Buffer *sourceBuffer, Buffer *destBuffer )	{ mixBuffers( sourceBuffer, destBuffer, std::min( sourceBuffer->getNumFrames(), destBuffer->getNumFrames() ) ); }
	//! Mixes \a numFrames frames of \a sourceBuffer to \a destBuffer's layout, replacing its content. Channel up or down mixing is applied if necessary.
	static void mixBuffers( const Buffer *sourceBuffer, Buffer *destBuffer, size_t numFrames );

	//! Sums \a sourceBuffer into \a destBuffer. Channel up or down mixing is applied if necessary. Unequal frame counts are permitted (the minimum size will be used).
	static void sumBuffers( const Buffer *sourceBuffer, Buffer *destBuffer )	{ sumBuffers( sourceBuffer, destBuffer, std::min( sourceBuffer->getNumFrames(), destBuffer->getNumFrames() ) ); }
	//! Sums \a numFrames frames of \a sourceBuffer into \a destBuffer. Channel up or down mixing is applied if necessary.
	static void sumBuffers( const Buffer *sourceBuffer, Buffer *destBuffer, size_t numFrames );

	size_t getSourceSampleRate() const			{ return mSourceSampleRate; }
	size_t getDestSampleRate() const			{ return mDestSampleRate; }
	size_t getSourceNumChannels() const			{ return mSourceNumChannels; }
	size_t getDestNumChannels() const			{ return mDestNumChannels; }
	size_t getSourceMaxFramesPerBlock() const	{ return mSourceMaxFramesPerBlock; }
	size_t getDestMaxFramesPerBlock() const		{ return mDestMaxFramesPerBlock; }

protected:
	Converter( size_t sourceSampleRate, size_t destSampleRate, size_t sourceNumChannels, size_t destNumChannels, size_t sourceMaxFramesPerBlock );

	size_t mSourceSampleRate, mDestSampleRate, mSourceNumChannels, mDestNumChannels, mSourceMaxFramesPerBlock, mDestMaxFramesPerBlock;
};

} } } // namespace cinder::audio2::dsp