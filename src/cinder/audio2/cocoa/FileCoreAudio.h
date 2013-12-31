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
#include "cinder/audio2/cocoa/CinderCoreAudio.h"

#include <AudioToolbox/ExtendedAudioFile.h>

namespace cinder { namespace audio2 { namespace cocoa {

class SourceFileCoreAudio : public SourceFile {
  public:
	SourceFileCoreAudio();
	SourceFileCoreAudio( const DataSourceRef &dataSource );
	virtual ~SourceFileCoreAudio();

	SourceFileRef	clone() const					override;

  private:
	void initImpl();

	size_t		performRead( Buffer *buffer, size_t bufferFrameOffset, size_t numFramesNeeded ) override;
	void		performSeek( size_t readPositionFrames )	override;
	bool		supportsConversion()						override	{ return true; }
	void		outputFormatUpdated()						override;

	std::shared_ptr<::OpaqueExtAudioFile>	mExtAudioFile; // TODO: use unique_ptr here and in TargetFileCoreAudio
	AudioBufferListShallowPtr				mBufferList;
	fs::path								mFilePath;
};

class TargetFileCoreAudio : public TargetFile {
  public:
	TargetFileCoreAudio( const DataTargetRef &dataTarget, size_t sampleRate, size_t numChannels, const std::string &extension );
	virtual ~TargetFileCoreAudio() {}

	void write( const Buffer *buffer, size_t frameOffset, size_t numFrames ) override;

  private:
	static ::AudioFileTypeID getFileTypeIdFromExtension( const std::string &ext );

	std::shared_ptr<::OpaqueExtAudioFile> mExtAudioFile;
	AudioBufferListShallowPtr mBufferList;
};

} } } // namespace cinder::audio2::cocoa
