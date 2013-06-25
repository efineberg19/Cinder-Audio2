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

#include "audio2/File.h"

#include "cinder/Cinder.h"

#if defined( CINDER_COCOA )
#include "audio2/cocoa/FileCoreAudio.h"
#elif defined( CINDER_MSW )
#include "audio2/msw/FileMediaFoundation.h"
#endif

namespace audio2 {

// TODO: this should be replaced with a genericized registrar derived from the ImageIo stuff.

SourceFileRef SourceFile::create(  ci::DataSourceRef dataSource, size_t numChannels, size_t sampleRate )
{
#if defined( CINDER_COCOA )
	return SourceFileRef( new cocoa::SourceFileCoreAudio( dataSource, numChannels, sampleRate ) );
#elif defined( CINDER_MSW )
	return SourceFileRef( new msw::SourceFileMediaFoundation( dataSource, numChannels, sampleRate ) );
#endif
}
	

} // namespace audio2