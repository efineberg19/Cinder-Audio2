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
#include "cinder/Utilities.h"

#if defined( CINDER_COCOA )
#include "audio2/cocoa/FileCoreAudio.h"
#elif defined( CINDER_MSW )
#include "audio2/msw/FileMediaFoundation.h"
#endif

#include "audio2/FileOggVorbis.h"

namespace cinder { namespace audio2 {

// TODO: these should be replaced with a generic registrar derived from the ImageIo stuff.

std::unique_ptr<SourceFile> SourceFile::create( const DataSourceRef &dataSource, size_t numChannels, size_t sampleRate )
{
	if( getPathExtension( dataSource->getFilePathHint() ) == "ogg" )
		return std::unique_ptr<SourceFile>( new SourceFileImplOggVorbis( dataSource, numChannels, sampleRate ) );

#if defined( CINDER_COCOA )
	return std::unique_ptr<SourceFile>( new cocoa::SourceFileImplCoreAudio( dataSource, numChannels, sampleRate ) );
#elif defined( CINDER_MSW )
	return std::unique_ptr<SourceFile>( new msw::SourceFileMediaFoundation( dataSource, numChannels, sampleRate ) );
#endif
}

std::unique_ptr<TargetFile> TargetFile::create( const DataTargetRef &dataTarget, size_t sampleRate, size_t numChannels, const std::string &extension )
{
	std::string ext = ( ! extension.empty() ? extension : getPathExtension( dataTarget->getFilePathHint() ) );

#if defined( CINDER_COCOA )
	return std::unique_ptr<TargetFile>( new cocoa::TargetFileImplCoreAudio( dataTarget, sampleRate, numChannels, ext ) );
#elif defined( CINDER_MSW )
	return std::unique_ptr<TargetFile>(); // TODO
//	return std::unique_ptr<TargetFile>( new msw::TargetFileMediaFoundation( dataTarget, numChannels, sampleRate, ext ) );
#endif
}

std::unique_ptr<TargetFile> TargetFile::create( const fs::path &path, size_t sampleRate, size_t numChannels, const std::string &extension )
{
	return create( (DataTargetRef)writeFile( path ), sampleRate, numChannels, extension );
}

} } // namespace cinder::audio2