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

#include "audio2/File.h"

// TODO: doc says this may be necessary on windows.  still necessary if we're static linking?
// - only really necessary if add source directly
#define OV_EXCLUDE_STATIC_CALLBACKS

#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

namespace cinder { namespace audio2 {

class SourceFileImplOggVorbis : public SourceFile {
  public:
	SourceFileImplOggVorbis( const DataSourceRef &dataSource, size_t numChannels, size_t sampleRate );
	virtual ~SourceFileImplOggVorbis();

	size_t		read( Buffer *buffer ) override;
	BufferRef	loadBuffer() override;
	void		seek( size_t readPosition ) override;

	// TODO: why have these? if user needs to new samplerate / #channels, why not create new SourceFileCoreAudio?
	// - I think it may have been due to the time at which default samplerate is known, which is probably no longer an issue
	void	setSampleRate( size_t sampleRate ) {}
	void	setNumChannels( size_t channels ) {}

  private:
	::OggVorbis_File mOggVorbisFile;
};

//class TargetFileImplOggVorbis : public TargetFile {
//public:
//	TargetFileImplOggVorbis( const DataTargetRef &dataTarget, size_t sampleRate, size_t numChannels, const std::string &extension );
//	virtual TargetFileImplOggVorbis() {}
//
//	void write( const Buffer *buffer, size_t frameOffset, size_t numFrames ) override;
//
//private:
//};

} } // namespace cinder::audio2
