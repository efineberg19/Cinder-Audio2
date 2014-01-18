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

#include "cinder/audio2/File.h"
#include "cinder/audio2/NodeSource.h"
#include "cinder/audio2/msw/util.h"

#include <vector>

struct IMFSourceReader;
struct IMFByteStream;

namespace cinder { namespace audio2 { namespace msw {

class SourceFileMediaFoundation : public SourceFile {
  public:
	enum Format { INT_16, FLOAT_32 }; // TODO: remove

	SourceFileMediaFoundation();
	SourceFileMediaFoundation( const DataSourceRef &dataSource );
	virtual ~SourceFileMediaFoundation();

	SourceFileRef	clone() const	override;

	size_t		performRead( Buffer *buffer, size_t bufferFrameOffset, size_t numFramesNeeded ) override;
	void		performSeek( size_t readPositionFrames ) override;

	//! Called automatically whenever a SourceFileMediaFoundation is constructed.
	static void		initMediaFoundation();
	//! This function is not called automatically, but users may if they wish to free up memory used by Media Foundation.
	static void		shutdownMediaFoundation();

  private:
	void		initReader();
	size_t		processNextReadSample();

	std::unique_ptr<::IMFSourceReader, ComReleaser>		mSourceReader;
	std::unique_ptr<ComIStream, ComReleaser>			mComIStream;
	std::unique_ptr<::IMFByteStream, ComReleaser>		mByteStream;
	DataSourceRef										mDataSource; // stored so that clone() can tell if original data source is a file or windows resource
	
	size_t			mBytesPerSample;
	Format			mSampleFormat;
	double			mSeconds;
	bool			mCanSeek;
	BufferDynamic	mReadBuffer;
	size_t			mReadBufferPos, mFramesRemainingInReadBuffer;

	static bool sIsMfInitialized;
};

} } } // namespace cinder::audio2::msw
