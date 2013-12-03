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

#include "cinder/audio2/FileOggVorbis.h"
#include "cinder/audio2/Exception.h"
#include "cinder/audio2/Debug.h"

using namespace std;

namespace cinder { namespace audio2 {

SourceFileImplOggVorbis::SourceFileImplOggVorbis( const DataSourceRef &dataSource, size_t sampleRate, size_t numChannels )
	: SourceFile( dataSource, sampleRate, numChannels )
{
	int status = ov_fopen( dataSource->getFilePath().string().c_str(), &mOggVorbisFile );
	if( status )
		throw AudioFileExc( string( "Failed to open Ogg Vorbis file with error: " ), (int32_t)status );


	LOG_V( "open success, ogg file info: " );
	// print comments plus a few lines about the bitstream we're decoding
	char **comment = ov_comment( &mOggVorbisFile, -1 )->user_comments;
	while( *comment )
		app::console() << *comment++ << endl;

	vorbis_info *info = ov_info( &mOggVorbisFile, -1 );
    mNumChannels = info->channels;
    mSampleRate = info->rate;

	app::console() << "\tversion: " << info->version << endl;
	app::console() << "\tBitstream is " << mNumChannels << " channel, " << mSampleRate << "Hz" << endl;
	app::console() << "\tEncoded by: " << ov_comment( &mOggVorbisFile, -1 )->vendor << endl;

	ogg_int64_t totalFrames = ov_pcm_total( &mOggVorbisFile, -1 );
    mNumFrames = static_cast<uint32_t>( totalFrames );
	app::console() << "\tframes: " << mNumFrames << endl;
}

SourceFileImplOggVorbis::~SourceFileImplOggVorbis()
{
	ov_clear( &mOggVorbisFile );
}

size_t SourceFileImplOggVorbis::read( Buffer *buffer )
{
	CI_ASSERT( buffer->getNumChannels() == mNumChannels );

	if( mReadPos >= mNumFrames )
		return 0;

	int numFramesToRead = (int)std::min( mNumFrames - mReadPos, std::min( mMaxFramesPerRead, buffer->getNumFrames() ) );
	int numFramesRead = 0;

	while( numFramesRead < numFramesToRead ) {
		float **outChannels;
		int section;
		long outNumFrames = ov_read_float( &mOggVorbisFile, &outChannels, numFramesToRead - numFramesRead, &section );
        if( outNumFrames <= 0 ) {
			if( outNumFrames < 0 )
				LOG_E( "stream error." );
            break;
		}

		for( int ch = 0; ch < mNumChannels; ch++ ) {
			float *channel = outChannels[ch];
			copy( channel, channel + outNumFrames, buffer->getChannel( ch ) + numFramesRead );
		}

		numFramesRead += outNumFrames;
		mReadPos += outNumFrames;
	}

	return numFramesRead;
}

BufferRef SourceFileImplOggVorbis::loadBuffer()
{
	if( mReadPos != 0 )
		seek( 0 );

	BufferRef result( new Buffer( mNumFrames, mNumChannels ) );

	while( true ) {
        float **outChannels;
		int section;
        long outNumFrames = ov_read_float( &mOggVorbisFile, &outChannels, (int)mMaxFramesPerRead, &section );
        if( outNumFrames <= 0 ) {
			if( outNumFrames < 0 )
				LOG_E( "stream error." );
            break;
		}
        else {
            for( int ch = 0; ch < mNumChannels; ch++ ) {
				float *channel = outChannels[ch];
				copy( channel, channel + outNumFrames, result->getChannel( ch ) + mReadPos );
            }
            mReadPos += outNumFrames;
		}
	}

	return result;
}

void SourceFileImplOggVorbis::seek( size_t readPositionFrames )
{
	if( readPositionFrames >= mNumFrames )
		return;

	int status = ov_pcm_seek( &mOggVorbisFile, (ogg_int64_t)readPositionFrames );
	CI_ASSERT( ! status );

	mReadPos = readPositionFrames;
}

} } // namespace cinder::audio2