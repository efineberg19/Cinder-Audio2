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

namespace cinder { namespace audio2 {
	
typedef std::shared_ptr<class SourceFile> SourceFileRef;
typedef std::shared_ptr<class TargetFile> TargetFileRef;

class SourceFile {
  public:
	static SourceFileRef create( const ci::DataSourceRef &dataSource, size_t numChannels, size_t sampleRate ); 

	virtual size_t	getSampleRate() const				{ return mSampleRate; }
	virtual void	setSampleRate( size_t sampleRate )	{ mSampleRate = sampleRate; }
	virtual size_t	getNumChannels() const				{ return mNumChannels; }
	virtual void	setNumChannels( size_t channels )	{ mNumChannels = channels; }
	virtual size_t	getNumFramesPerRead() const			{ return mNumFramesPerRead; }
	virtual void	setNumFramesPerRead( size_t count )	{ mNumFramesPerRead = count; }

	virtual size_t	getNumFrames() const					{ return mNumFrames; }
	virtual size_t	getFileNumChannels() const				{ return mFileNumChannels; }
	virtual size_t	getFileSampleRate() const				{ return mFileSampleRate; }

	//! \brief loads either as many frames as \t buffer can hold, or as many as there are left. \return number of frames loaded.
	virtual size_t read( Buffer *buffer ) = 0;

	virtual BufferRef loadBuffer() = 0;

	// TODO: should be able to seek by both frames and milliseconds - find the best way to allow this
	// - overloading would be easy to make mistakes, should use different method names
	virtual void seek( size_t readPosition ) = 0;


  protected:
	SourceFile( const ci::DataSourceRef &dataSource, size_t numChannels, size_t sampleRate )
	: mFileSampleRate( 0 ), mFileNumChannels( 0 ), mNumFrames( 0 ), mNumChannels( numChannels ), mSampleRate( sampleRate ), mNumFramesPerRead( 4096 )
	{}

	size_t mSampleRate, mNumChannels, mNumFrames, mFileSampleRate, mFileNumChannels, mNumFramesPerRead;
};

class TargetFile {

};

//BufferRef loadAudio( SourceFileRef sourcefile );

} } // namespace cinder::audio2