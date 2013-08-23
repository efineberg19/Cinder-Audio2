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
#include "audio2/GeneratorNode.h"
#include "audio2/cocoa/CinderCoreAudio.h"

#include <AudioToolbox/ExtendedAudioFile.h>

namespace cinder { namespace audio2 { namespace cocoa {

class SourceFileCoreAudio : public SourceFile {
  public:
	SourceFileCoreAudio( ci::DataSourceRef dataSource, size_t numChannels = 0, size_t sampleRate = 0 );

	size_t		read( Buffer *buffer ) override;
	BufferRef	loadBuffer() override;
	void		seek( size_t readPosition ) override;

	void	setSampleRate( size_t sampleRate ) override;
	void	setNumChannels( size_t channels ) override;

  private:
	void updateOutputFormat();
	
	std::shared_ptr<::OpaqueExtAudioFile> mExtAudioFile;
	AudioBufferListShallowPtr mBufferList;
	size_t mReadPos;
};

} } } // namespace cinder::audio2::cocoa
