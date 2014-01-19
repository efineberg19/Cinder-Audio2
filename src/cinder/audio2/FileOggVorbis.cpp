/*
 Copyright (c) 2014, The Cinder Project

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

#include "cinder/audio2/FileOggVorbis.h"
#include "cinder/audio2/dsp/Converter.h"
#include "cinder/audio2/Exception.h"
#include "cinder/audio2/Debug.h"

#include <sstream>

using namespace std;

namespace cinder { namespace audio2 {

SourceFileImplOggVorbis::SourceFileImplOggVorbis()
	: SourceFile()
{}

SourceFileImplOggVorbis::SourceFileImplOggVorbis( const DataSourceRef &dataSource )
	: SourceFile()
{
	if( dataSource->isFilePath() ) {
		mFilePath = dataSource->getFilePath();
		initImpl();
	}
	else {
		// TODO: to enable this, need to use ov_open_callbacks + ov_callbacks and cinders datasource streaming support
		CI_ASSERT( 0 && "loading from win resource not implemented" );
	}
}

SourceFileRef SourceFileImplOggVorbis::clone() const
{
	shared_ptr<SourceFileImplOggVorbis> result( new SourceFileImplOggVorbis );

	result->mFilePath = mFilePath;
	result->initImpl();

	return result;
}

SourceFileImplOggVorbis::~SourceFileImplOggVorbis()
{
	ov_clear( &mOggVorbisFile );
}

void SourceFileImplOggVorbis::initImpl()
{
	int status = ov_fopen( mFilePath.string().c_str(), &mOggVorbisFile );
	if( status )
		throw AudioFileExc( string( "Failed to open Ogg Vorbis file with error: " ), (int32_t)status );

	vorbis_info *info = ov_info( &mOggVorbisFile, -1 );
    mSampleRate = mNativeSampleRate = info->rate;
    mNumChannels = mNativeNumChannels = info->channels;

	ogg_int64_t totalFrames = ov_pcm_total( &mOggVorbisFile, -1 );
    mNumFrames = mFileNumFrames = static_cast<uint32_t>( totalFrames );
}

size_t SourceFileImplOggVorbis::performRead( Buffer *buffer, size_t bufferFrameOffset, size_t numFramesNeeded )
{
	CI_ASSERT( buffer->getNumFrames() >= bufferFrameOffset + numFramesNeeded );

	long readCount = 0;
	while( readCount < numFramesNeeded ) {
		float **outChannels;
		int section;

		long outNumFrames = ov_read_float( &mOggVorbisFile, &outChannels, int( numFramesNeeded - readCount ), &section );
		if( outNumFrames <= 0 ) {
			if( outNumFrames < 0 )
				throw AudioFileExc( "ov_read_float error", (int32_t)outNumFrames );

			break;
		}

		size_t offset = bufferFrameOffset + readCount;
		for( int ch = 0; ch < mNativeNumChannels; ch++ ) {
			float *channel = outChannels[ch];
			copy( channel, channel + outNumFrames, buffer->getChannel( ch ) + offset );
		}

		readCount += outNumFrames;
	}

	return static_cast<size_t>( readCount );
}

void SourceFileImplOggVorbis::performSeek( size_t readPositionFrames )
{
	int status = ov_pcm_seek( &mOggVorbisFile, (ogg_int64_t)readPositionFrames );
	CI_ASSERT( ! status );
}

string SourceFileImplOggVorbis::getMetaData() const
{
	ostringstream str;
	const auto vf = const_cast<OggVorbis_File *>( &mOggVorbisFile );

	str << "encoded by: " << ov_comment( vf, -1 )->vendor << endl;
	str << "comments: " << endl;
	char **comment = ov_comment( vf, -1 )->user_comments;
	while( *comment )
		str << *comment++ << endl;

	return str.str();
}

} } // namespace cinder::audio2