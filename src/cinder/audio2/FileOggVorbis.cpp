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
	mDataSource = dataSource;
	init();
}

SourceFileRef SourceFileImplOggVorbis::clone() const
{
	shared_ptr<SourceFileImplOggVorbis> result( new SourceFileImplOggVorbis );

	result->mDataSource = mDataSource;
	result->init();

	return result;
}

SourceFileImplOggVorbis::~SourceFileImplOggVorbis()
{
	ov_clear( &mOggVorbisFile );
}

void SourceFileImplOggVorbis::init()
{
	CI_ASSERT( mDataSource );
	if( mDataSource->isFilePath() ) {
		int status = ov_fopen( mDataSource->getFilePath().string().c_str(), &mOggVorbisFile );
		if( status )
			throw AudioFileExc( string( "Failed to open Ogg Vorbis file with error: " ), (int32_t)status );
	}
	else {
		mStream = mDataSource->createStream();

		ov_callbacks callbacks;
		callbacks.read_func = readFn;
		callbacks.seek_func = seekFn;
		callbacks.close_func = closeFn;
		callbacks.tell_func = tellFn;

		int status = ov_open_callbacks( this, &mOggVorbisFile, NULL, 0, callbacks );
		CI_ASSERT( status == 0 );
	}

	vorbis_info *info = ov_info( &mOggVorbisFile, -1 );
    mSampleRate = mNativeSampleRate = info->rate;
    mNumChannels = mNativeNumChannels = info->channels;

	ogg_int64_t totalFrames = ov_pcm_total( &mOggVorbisFile, -1 );
    mNumFrames = mFileNumFrames = static_cast<uint32_t>( totalFrames );
}

size_t SourceFileImplOggVorbis::performRead( Buffer *buffer, size_t bufferFrameOffset, size_t numFramesNeeded )
{
	CI_ASSERT( buffer->getNumFrames() >= bufferFrameOffset + numFramesNeeded );

	size_t readCount = 0;
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
		for( size_t ch = 0; ch < mNativeNumChannels; ch++ )
			memcpy( buffer->getChannel( ch ) + offset, outChannels[ch], outNumFrames * sizeof( float ) );

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

// static
size_t SourceFileImplOggVorbis::readFn( void *ptr, size_t size, size_t nmemb, void *datasource )
{
	auto sourceFile = (SourceFileImplOggVorbis *)datasource;

	size_t bytesRead = sourceFile->mStream->readDataAvailable( ptr, size * nmemb);
	return bytesRead / size;
}

// static 
int SourceFileImplOggVorbis::seekFn( void *datasource, ogg_int64_t offset, int whence )
{
	auto sourceFile = (SourceFileImplOggVorbis *)datasource;

	switch( whence ) {
		case SEEK_SET:
			sourceFile->mStream->seekAbsolute( (off_t)offset );
			break;
		case SEEK_CUR:
			sourceFile->mStream->seekRelative( (off_t)offset );
			break;
		case SEEK_END:
			sourceFile->mStream->seekAbsolute( sourceFile->mStream->size() );
			break;
		default:
			CI_ASSERT_NOT_REACHABLE();
			return -1;
	}

	return 0;
}

// static
int	SourceFileImplOggVorbis::closeFn( void *datasource )
{
	return 0;
}

// static
long SourceFileImplOggVorbis::tellFn( void *datasource )
{
	auto sourceFile = (SourceFileImplOggVorbis *)datasource;
	
	return sourceFile->mStream->tell();
}

} } // namespace cinder::audio2