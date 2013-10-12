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

#include "audio2/Buffer.h"

#include "cinder/DataSource.h"
#include "cinder/DataTarget.h"

// TODO: switch channels and samplerate order for consistency with nodess
// - alternatively, make optional Options struct, where file params are used by default

namespace cinder { namespace audio2 {
	
typedef std::shared_ptr<class SourceFile> SourceFileRef;
typedef std::shared_ptr<class TargetFile> TargetFileRef;

class SourceFile {
  public:
	static std::unique_ptr<SourceFile> create( const DataSourceRef &dataSource, size_t numChannels, size_t sampleRate );
	virtual ~SourceFile() {}

	virtual size_t	getSampleRate() const				{ return mSampleRate; }
	virtual size_t	getNumChannels() const				{ return mNumChannels; }
	virtual size_t	getFileSampleRate() const				{ return mFileSampleRate; }
	virtual size_t	getFileNumChannels() const				{ return mFileNumChannels; }

	virtual size_t	getNumFrames() const					{ return mNumFrames; }

	virtual void	setMaxFramesPerRead( size_t count )	{ mMaxFramesPerRead = count; }
	virtual size_t	getMaxFramesPerRead() const			{ return mMaxFramesPerRead; }

	//! \brief loads either as many frames as \t buffer can hold, or as many as there are left. \return number of frames loaded.
	virtual size_t read( Buffer *buffer ) = 0;

	virtual BufferRef loadBuffer() = 0;

	//! Seek the read position to \a readPositionFrames
	virtual void seek( size_t readPositionFrames ) = 0;
	//! Seek to read position \a readPositionSeconds
	void seekToTime( double readPositionSeconds )	{ return seek( size_t( readPositionSeconds * (double)getFileSampleRate() ) ); }

  protected:
	SourceFile( const DataSourceRef &dataSource, size_t numChannels, size_t sampleRate )
	: mFileSampleRate( 0 ), mFileNumChannels( 0 ), mNumFrames( 0 ), mNumChannels( numChannels ), mSampleRate( sampleRate ), mMaxFramesPerRead( 4096 )
	{}

	size_t mSampleRate, mNumChannels, mNumFrames, mFileSampleRate, mFileNumChannels, mMaxFramesPerRead;
};

// TODO: support sample formats other than float

class TargetFile {
  public:
	static std::unique_ptr<TargetFile> create( const DataTargetRef &dataTarget, size_t sampleRate, size_t numChannels, const std::string &extension = "" );
	static std::unique_ptr<TargetFile> create( const fs::path &path, size_t sampleRate, size_t numChannels, const std::string &extension = "" );
	virtual ~TargetFile() {}

	//! If default numFrames is used (0), will write all frames in \a buffer
	virtual void write( const Buffer *buffer, size_t frameOffset = 0, size_t numFrames = 0 ) = 0;

  protected:
	TargetFile( const DataTargetRef &dataTarget, size_t sampleRate, size_t numChannels )
	: mSampleRate( sampleRate ), mNumChannels( numChannels )
	{}

	size_t mSampleRate, mNumChannels;
};

//BufferRef loadAudio( SourceFileRef sourcefile );

} } // namespace cinder::audio2