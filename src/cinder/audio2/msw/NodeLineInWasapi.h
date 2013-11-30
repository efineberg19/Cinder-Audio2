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

#if( _WIN32_WINNT < _WIN32_WINNT_VISTA )
	#error "WASAPI unsupported for deployment target less than Windows Vista"
#endif

#include "cinder/audio2/Device.h"
#include "cinder/audio2/NodeSource.h"

namespace cinder { namespace audio2 { namespace msw {

class LineInWasapi : public LineIn {
  public:
	LineInWasapi( const DeviceRef &device, const Format &format = Format() );
	virtual ~LineInWasapi();

	std::string virtual getTag()				{ return "LineInWasapi"; }

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	uint64_t getLastUnderrun() override;
	uint64_t getLastOverrun() override;

	void process( Buffer *buffer ) override;

  private:

	struct Impl;
	std::unique_ptr<Impl> mImpl;
	BufferInterleaved mInterleavedBuffer;

	size_t mCaptureBlockSize; // per channel. TODO: this should be user settable
};

} } } // namespace cinder::audio2::msw