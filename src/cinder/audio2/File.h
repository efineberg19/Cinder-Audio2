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

#include "cinder/audio2/Buffer.h"

#include "cinder/DataSource.h"
#include "cinder/DataTarget.h"

namespace cinder { namespace audio2 {
	
typedef std::shared_ptr<class Source>			SourceRef;
typedef std::shared_ptr<class SourceFile>		SourceFileRef;
typedef std::shared_ptr<class TargetFile>		TargetFileRef;

class Source {
  public:
	virtual ~Source();
	//! Returns the output samplerate (the samplerate of frames read).
	size_t	getSampleRate() const				{ return mSampleRate; }
	//! Returns the output number of channels (the num channnels of frames read).
	size_t	getNumChannels() const				{ return mNumChannels; }
	//! Returns the true samplerate of the Source. \note Actual output samplerate may differ. \see getSampleRate()
	size_t	getNativeSampleRate() const			{ return mNativeSampleRate; }
	//! Returns the true number of channels of the Source. \note Actual output num channels may differ. \see getNumChannels()
	size_t	getNativeNumChannels() const		{ return mNativeNumChannels; }
	//! Returns the maximum number of frames that can be read in one chunk.
	size_t	getMaxFramesPerRead() const			{ return mMaxFramesPerRead; }
	//! Sets the maximum number of frames that can be read in one chunk.
	virtual void	setMaxFramesPerRead( size_t count )		{ mMaxFramesPerRead = count; }

	//! Sets the output format options \a outputSampleRate and optionally \a outputNumChannels (default will use the Source's num channels),
	//!	allowing output samplerate and channel count to be different from the actual Source.
	//! \note All objects referencing this Source will be effected, so users should create a copy of the Source for each context or situation where it needs to control the format.
	void setOutputFormat( size_t outputSampleRate, size_t outputNumChannels = 0 );
	//! Loads either as many frames as \t buffer can hold, or as many as there are left. \return number of frames loaded.
	virtual size_t read( Buffer *buffer ) = 0;

	virtual std::string getMetaData() const	{ return std::string(); }

  protected:
	Source();

	//! Called at the end of setOutputFormat(). Subclasses must implement this to account for samplerate or channel conversions, if needed.
	virtual void outputFormatUpdated() = 0;

	size_t mNativeSampleRate, mNativeNumChannels, mSampleRate, mNumChannels, mMaxFramesPerRead;
};

class SourceFile : public Source {
  public:
	//! Creates a new SourceFile from \a dataSource, setting the samplerate and num channels to match the native values.
	static std::unique_ptr<SourceFile> create( const DataSourceRef &dataSource );
	virtual ~SourceFile()	{}

	virtual size_t	getNumFrames() const					{ return mNumFrames; }

	//! Returns the length in seconds when played back at the specified samplerate.
	double getNumSeconds() const	{ return (double)getNumFrames() / (double)mSampleRate; }

	virtual BufferRef loadBuffer() = 0;
	//! Seek the read position to \a readPositionFrames
	virtual void seek( size_t readPositionFrames ) = 0;
	//! Seek to read position \a readPositionSeconds
	virtual void seekToTime( double readPositionSeconds )	{ return seek( size_t( readPositionSeconds * (double)getNativeSampleRate() ) ); }

  protected:
	SourceFile( const DataSourceRef &dataSource ) : Source(), mNumFrames( 0 )
	{}

	size_t mNumFrames;
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

//! Convenience method for loading a SourceFile from \a dataSource. \see SourceFile::create()
inline std::unique_ptr<SourceFile>	load( const DataSourceRef &dataSource )	{ return SourceFile::create( dataSource ); }

} } // namespace cinder::audio2